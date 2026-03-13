#!/usr/bin/env python3
"""
Generates a waveform PNG (C) and self-contained HTML report (A)
from the UART output and Renode peripheral log.

Two independent data sources:
  1. UART log  — what the firmware *claims* GPIO 25 is doing (printf)
  2. Renode log — what the emulator *actually observed* at the SIO registers

RP2040 SIO register offsets for GPIO output:
  0x010  GPIO_OUT      (direct write)
  0x014  GPIO_OUT_SET  (set bits)
  0x018  GPIO_OUT_CLR  (clear bits)
  0x01C  GPIO_OUT_XOR  (toggle bits)

Bit 25 = 0x02000000 = LED pin
"""
import re
import sys
import os
import base64
from datetime import datetime

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


# ── RP2040 SIO constants ────────────────────────────────────────────
GPIO_OUT_SET_OFFSET = 0x014
GPIO_OUT_CLR_OFFSET = 0x018
GPIO_OUT_XOR_OFFSET = 0x01C
GPIO_OUT_OFFSET     = 0x010
LED_BIT             = 1 << 25   # 0x02000000


# ── UART log parsing (firmware claims) ──────────────────────────────

def parse_uart_log(filepath):
    events = []
    checks = {
        'boot': False,
        'uart_init': False,
        'led_cycles': 0,
        'test_complete': False,
    }

    with open(filepath) as f:
        for line in f:
            line = line.strip()
            m = re.match(r'\[(\d+)\] (LED ON|LED OFF)\s+-\s+cycle (\d+)', line)
            if m:
                events.append({
                    'time_us': int(m.group(1)),
                    'state': m.group(2),
                    'cycle': int(m.group(3)),
                })

            if 'BOOT: RP2040 Digital Twin POC' in line:
                checks['boot'] = True
            if 'UART initialized successfully' in line:
                checks['uart_init'] = True
            if 'LED ON' in line:
                checks['led_cycles'] += 1
            if 'TEST COMPLETE' in line:
                checks['test_complete'] = True

    return events, checks


# ── Renode SIO log parsing (emulator ground truth) ──────────────────

def parse_renode_log(filepath):
    """
    Parse the Renode log for SIO peripheral accesses that affect GPIO 25.

    Renode LogPeripheralAccess lines look like one of these patterns:
      HH:MM:SS.FFFF [DEBUG] sysbus.sio: WriteUInt32 to 0x14, value 0x2000000
      HH:MM:SS.FFFF [DEBUG] sio: WriteLong at 0x14, value 0x2000000.
      HH:MM:SS.FFFF [DEBUG] sysbus.sio: [cpu0] WriteDoubleWord to 0x14, value 0x2000000.

    We look for writes to offsets 0x10/0x14/0x18/0x1C and check bit 25.
    """
    gpio_events = []
    current_gpio25_state = 0  # assume LOW at boot

    if not os.path.exists(filepath):
        return gpio_events, False

    # Match Renode log lines like:
    # 13:01:16.7377 [INFO] sio: [cpu0: 0x10000408] WriteUInt32 to 0x18 (GPIO_OUT_CLR), value 0x2000000.
    # Captures: timestamp, offset (hex), register name, value (hex)
    write_re = re.compile(
        r'(\d{2}:\d{2}:\d{2}\.\d+)\s+'
        r'\[.*?\]\s+'
        r'(?:sysbus\.)?sio:\s+'
        r'(?:\[.*?\]\s+)?'                         # optional [cpu0: 0xADDR]
        r'Write\w+\s+to\s+'
        r'0x([0-9A-Fa-f]+)\s+'
        r'\((\w+)\),?\s+'
        r'value\s+0x([0-9A-Fa-f]+)',
        re.IGNORECASE
    )

    with open(filepath) as f:
        for line in f:
            m = write_re.search(line)
            if not m:
                continue

            timestamp_str = m.group(1)
            offset = int(m.group(2), 16)
            reg_name = m.group(3)
            value  = int(m.group(4), 16)

            # Only care about GPIO output registers
            if offset not in (GPIO_OUT_OFFSET, GPIO_OUT_SET_OFFSET,
                              GPIO_OUT_CLR_OFFSET, GPIO_OUT_XOR_OFFSET):
                continue

            # Only care about writes that touch bit 25
            if not (value & LED_BIT):
                continue

            # Determine new GPIO 25 state
            if offset == GPIO_OUT_SET_OFFSET:
                new_state = 1
            elif offset == GPIO_OUT_CLR_OFFSET:
                new_state = 0
            elif offset == GPIO_OUT_XOR_OFFSET:
                new_state = 1 - current_gpio25_state
            elif offset == GPIO_OUT_OFFSET:
                new_state = 1 if (value & LED_BIT) else 0
            else:
                continue

            # Only record actual transitions
            if new_state != current_gpio25_state:
                # Parse timestamp HH:MM:SS.FFFF → total microseconds
                parts = timestamp_str.split(':')
                secs_parts = parts[2].split('.')
                total_us = (
                    int(parts[0]) * 3600_000_000
                    + int(parts[1]) * 60_000_000
                    + int(secs_parts[0]) * 1_000_000
                    + int(secs_parts[1].ljust(6, '0')[:6])
                )
                gpio_events.append({
                    'time_us_abs': total_us,
                    'time_us': 0,  # filled in below
                    'state': 'HIGH' if new_state else 'LOW',
                    'register': f'0x{offset:03X}',
                    'value': f'0x{value:08X}',
                })
                current_gpio25_state = new_state

    # Make timestamps relative to first event (emulation-relative)
    if gpio_events:
        t0 = gpio_events[0]['time_us_abs']
        for e in gpio_events:
            e['time_us'] = e['time_us_abs'] - t0

    return gpio_events, True


def cross_reference(uart_events, gpio_events):
    """
    Compare UART claims against emulator-observed GPIO transitions.
    Returns a list of comparison rows and an overall match boolean.
    """
    rows = []
    match = True

    # Pair up by index — both should have the same number of transitions
    max_len = max(len(uart_events), len(gpio_events))
    for i in range(max_len):
        uart_e = uart_events[i] if i < len(uart_events) else None
        gpio_e = gpio_events[i] if i < len(gpio_events) else None

        if uart_e and gpio_e:
            uart_state = 'HIGH' if uart_e['state'] == 'LED ON' else 'LOW'
            ok = uart_state == gpio_e['state']
            rows.append({
                'index': i,
                'uart_time_us': uart_e['time_us'],
                'uart_state': uart_state,
                'gpio_time_us': gpio_e['time_us'],
                'gpio_state': gpio_e['state'],
                'gpio_reg': gpio_e['register'],
                'match': ok,
            })
            if not ok:
                match = False
        elif uart_e:
            rows.append({
                'index': i,
                'uart_time_us': uart_e['time_us'],
                'uart_state': 'HIGH' if uart_e['state'] == 'LED ON' else 'LOW',
                'gpio_time_us': None,
                'gpio_state': '(missing)',
                'gpio_reg': '-',
                'match': False,
            })
            match = False
        else:
            rows.append({
                'index': i,
                'uart_time_us': None,
                'uart_state': '(missing)',
                'gpio_time_us': gpio_e['time_us'],
                'gpio_state': gpio_e['state'],
                'gpio_reg': gpio_e['register'],
                'match': False,
            })
            match = False

    return rows, match


# ── Waveform generation ─────────────────────────────────────────────

def generate_waveform(uart_events, gpio_events, output_path):
    if not uart_events and not gpio_events:
        print("[report] No events found — skipping waveform")
        return False

    has_gpio = len(gpio_events) > 0
    nrows = 2 if has_gpio else 1
    fig, axes = plt.subplots(nrows, 1, figsize=(13, 2.4 * nrows + 0.4),
                             sharex=True)
    if nrows == 1:
        axes = [axes]

    fig.patch.set_facecolor('#0f0f23')

    def plot_trace(ax, events, time_key, state_map, title, color):
        ax.set_facecolor('#0f0f23')
        times = [0.0]
        vals  = [0]
        for e in events:
            times.append(e[time_key] / 1000.0)
            vals.append(state_map(e))
        times.append(times[-1] + 100)
        vals.append(vals[-1])

        ax.step(times, vals, where='post', color=color, linewidth=2)
        ax.fill_between(times, vals, step='post', alpha=0.25, color=color)
        ax.set_ylabel('GPIO 25', color='#a0a0c0', fontsize=9)
        ax.set_yticks([0, 1])
        ax.set_yticklabels(['LOW', 'HIGH'], color='#a0a0c0', fontsize=8)
        ax.set_ylim(-0.3, 1.4)
        ax.set_title(title, color='#00d4ff', fontsize=10, pad=6, loc='left')
        ax.tick_params(colors='#a0a0c0')
        for spine in ax.spines.values():
            spine.set_edgecolor('#333355')
        ax.grid(True, axis='x', color='#333355', linewidth=0.5)

    # Top: UART-reported (firmware claims)
    plot_trace(axes[0], uart_events, 'time_us',
               lambda e: 1 if e['state'] == 'LED ON' else 0,
               'Firmware-reported (UART printf)', '#f1c40f')

    # Bottom: Emulator-observed (SIO register writes)
    if has_gpio:
        plot_trace(axes[1], gpio_events, 'time_us',
                   lambda e: 1 if e['state'] == 'HIGH' else 0,
                   'Emulator-observed (SIO register writes)', '#2ecc71')

    axes[-1].set_xlabel('Time (ms)', color='#a0a0c0')
    axes[-1].set_xlim(left=-20)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight',
                facecolor=fig.get_facecolor())
    plt.close()
    print(f"[report] Waveform saved → {output_path}")
    return True


# ── HTML report ─────────────────────────────────────────────────────

def generate_html(uart_file, checks, uart_events, gpio_events,
                  xref_rows, xref_match, gpio_log_found,
                  waveform_png, output_path):
    with open(uart_file) as f:
        raw_output = f.read().strip()

    with open(waveform_png, 'rb') as f:
        png_b64 = base64.b64encode(f.read()).decode()

    # ── Validation checks ──
    check_rows = [
        ('Boot message received',         checks['boot']),
        ('UART initialized',              checks['uart_init']),
        ('LED cycles (5 expected)',        checks['led_cycles'] == 5),
        ('Test completion message',        checks['test_complete']),
    ]
    if gpio_log_found:
        check_rows.append(('GPIO transitions match UART claims', xref_match))
        check_rows.append((f'Emulator GPIO events ({len(gpio_events)} observed)',
                           len(gpio_events) == 10))

    all_pass = all(v for _, v in check_rows)
    status_class = 'pass' if all_pass else 'fail'
    status_text  = '&#10003; ALL CHECKS PASSED' if all_pass else '&#10007; SOME CHECKS FAILED'

    checks_html = ''
    for name, passed in check_rows:
        badge = ('<span class="badge pass">PASS</span>' if passed
                 else '<span class="badge fail">FAIL</span>')
        checks_html += f'    <tr><td>{name}</td><td>{badge}</td></tr>\n'

    # ── Cross-reference table ──
    xref_html = ''
    if xref_rows:
        for r in xref_rows:
            u_t = f'{r["uart_time_us"] / 1000:.1f} ms' if r['uart_time_us'] is not None else '-'
            g_t = f'{r["gpio_time_us"] / 1000:.1f} ms' if r['gpio_time_us'] is not None else '-'
            match_class = 'ev-on' if r['match'] else 'ev-off'
            match_text  = 'MATCH' if r['match'] else 'MISMATCH'
            xref_html += (
                f'    <tr>'
                f'<td>{r["index"]}</td>'
                f'<td>{u_t}</td><td>{r["uart_state"]}</td>'
                f'<td>{g_t}</td><td>{r["gpio_state"]}</td>'
                f'<td>{r["gpio_reg"]}</td>'
                f'<td class="{match_class}">{match_text}</td>'
                f'</tr>\n'
            )

    # ── Event timeline table ──
    events_html = ''
    for e in uart_events:
        state_class = 'ev-on' if e['state'] == 'LED ON' else 'ev-off'
        events_html += (
            f'    <tr>'
            f'<td>{e["time_us"] / 1000:.1f} ms</td>'
            f'<td class="{state_class}">{e["state"]}</td>'
            f'<td>Cycle {e["cycle"]}</td>'
            f'</tr>\n'
        )

    now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # ── Build cross-reference card ──
    xref_card = ''
    if gpio_log_found and xref_rows:
        xref_card = f"""
  <div class="card full">
    <h2>Cross-Reference: Firmware Claims vs Emulator GPIO</h2>
    <p class="card-note">Compares UART printf output against actual SIO register writes observed by Renode.</p>
    <table>
      <tr><th>#</th><th>UART Time</th><th>UART State</th><th>GPIO Time</th><th>GPIO State</th><th>Register</th><th>Verdict</th></tr>
{xref_html}    </table>
  </div>"""
    elif not gpio_log_found:
        xref_card = """
  <div class="card full">
    <h2>Cross-Reference: Firmware Claims vs Emulator GPIO</h2>
    <p class="card-note warn">Renode log (renode.log) not found. Enable <code>logFile</code> and
    <code>LogPeripheralAccess sysbus.sio</code> in the .resc script to verify GPIO state independently.</p>
  </div>"""

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>RP2040 Digital Twin — Test Report</title>
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{ font-family: 'Segoe UI', system-ui, sans-serif; background: #0d0d1a; color: #dde1f0; padding: 28px; }}
  h1 {{ color: #00d4ff; font-size: 1.6em; border-bottom: 2px solid #00d4ff22; padding-bottom: 10px; margin-bottom: 6px; }}
  h2 {{ color: #7eb8f7; font-size: 1.05em; margin-bottom: 12px; text-transform: uppercase; letter-spacing: .06em; }}
  .meta {{ color: #666; font-size: 0.82em; margin-bottom: 24px; }}
  .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 18px; margin-bottom: 18px; }}
  .card {{ background: #141428; border: 1px solid #252545; border-radius: 8px; padding: 18px 20px; }}
  .card.full {{ grid-column: 1 / -1; }}
  .card-note {{ font-size: 0.82em; color: #888; margin-bottom: 10px; }}
  .card-note.warn {{ color: #e67e22; }}
  .status {{ font-size: 1.25em; font-weight: 700; padding: 10px 18px; border-radius: 6px; display: inline-block; margin-top: 4px; }}
  .status.pass {{ background: #0d2e1a; color: #2ecc71; border: 1px solid #2ecc7155; }}
  .status.fail {{ background: #2e0d0d; color: #e74c3c; border: 1px solid #e74c3c55; }}
  table {{ border-collapse: collapse; width: 100%; font-size: 0.9em; }}
  th, td {{ padding: 7px 12px; border: 1px solid #252545; text-align: left; }}
  th {{ background: #1a1a35; color: #7eb8f7; }}
  .badge {{ font-weight: 700; font-size: 0.8em; padding: 2px 8px; border-radius: 4px; }}
  .badge.pass {{ background: #0d2e1a; color: #2ecc71; }}
  .badge.fail {{ background: #2e0d0d; color: #e74c3c; }}
  .ev-on  {{ color: #2ecc71; font-weight: 600; }}
  .ev-off {{ color: #e74c3c; font-weight: 600; }}
  pre {{ background: #090917; border: 1px solid #252545; padding: 14px; border-radius: 6px;
         font-size: 0.82em; color: #90d090; overflow-x: auto; white-space: pre-wrap; }}
  img {{ width: 100%; border-radius: 6px; border: 1px solid #252545; }}
</style>
</head>
<body>
<h1>RP2040 Digital Twin &mdash; Test Report</h1>
<p class="meta">Generated: {now} &nbsp;|&nbsp; Firmware: LED blink &times;5 &nbsp;|&nbsp; Platform: Renode 1.15.3 (emulated RP2040)</p>

<div class="grid">

  <div class="card">
    <h2>Overall Result</h2>
    <div class="status {status_class}">{status_text}</div>
  </div>

  <div class="card">
    <h2>Validation Checks</h2>
    <table>
      <tr><th>Check</th><th>Result</th></tr>
{checks_html}    </table>
  </div>

  <div class="card full">
    <h2>GPIO 25 Waveform</h2>
    <img src="data:image/png;base64,{png_b64}" alt="LED waveform">
  </div>
{xref_card}
  <div class="card">
    <h2>UART Event Timeline</h2>
    <table>
      <tr><th>Time</th><th>State</th><th>Event</th></tr>
{events_html}    </table>
  </div>

  <div class="card">
    <h2>Raw UART Output</h2>
    <pre>{raw_output}</pre>
  </div>

</div>
</body>
</html>"""

    with open(output_path, 'w') as f:
        f.write(html)
    print(f"[report] HTML report saved → {output_path}")
    return all_pass


# ── main ────────────────────────────────────────────────────────────

if __name__ == '__main__':
    output_dir   = sys.argv[1] if len(sys.argv) > 1 else '/workspace/output'
    uart_file    = os.path.join(output_dir, 'uart_output.txt')
    renode_log   = os.path.join(output_dir, 'renode.log')
    waveform_png = os.path.join(output_dir, 'waveform.png')
    report_html  = os.path.join(output_dir, 'report.html')

    if not os.path.exists(uart_file):
        print(f"[report] ERROR: {uart_file} not found")
        sys.exit(1)

    # Parse both sources
    uart_events, checks = parse_uart_log(uart_file)
    gpio_events, gpio_log_found = parse_renode_log(renode_log)

    if gpio_log_found:
        print(f"[report] Parsed {len(gpio_events)} GPIO transitions from Renode log")
    else:
        print(f"[report] No Renode log found — GPIO verification unavailable")

    # Cross-reference
    xref_rows, xref_match = cross_reference(uart_events, gpio_events)

    # Generate outputs
    ok = generate_waveform(uart_events, gpio_events, waveform_png)
    if ok:
        all_pass = generate_html(
            uart_file, checks, uart_events, gpio_events,
            xref_rows, xref_match, gpio_log_found,
            waveform_png, report_html)
        sys.exit(0 if all_pass else 1)
    else:
        sys.exit(1)
