#!/usr/bin/env python3
"""
Generates a waveform PNG and self-contained HTML report from the UART output
and Renode peripheral log.

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
import json
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
    gpio_events = []
    current_gpio25_state = 0  # assume LOW at boot

    if not os.path.exists(filepath):
        return gpio_events, False

    write_re = re.compile(
        r'(\d{2}:\d{2}:\d{2}\.\d+)\s+'
        r'\[.*?\]\s+'
        r'(?:sysbus\.)?sio:\s+'
        r'(?:\[.*?\]\s+)?'
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
            value  = int(m.group(4), 16)

            if offset not in (GPIO_OUT_OFFSET, GPIO_OUT_SET_OFFSET,
                              GPIO_OUT_CLR_OFFSET, GPIO_OUT_XOR_OFFSET):
                continue

            if not (value & LED_BIT):
                continue

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

            if new_state != current_gpio25_state:
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
                    'time_us': 0,
                    'state': 'HIGH' if new_state else 'LOW',
                    'register': f'0x{offset:03X}',
                    'value': f'0x{value:08X}',
                })
                current_gpio25_state = new_state

    if gpio_events:
        t0 = gpio_events[0]['time_us_abs']
        for e in gpio_events:
            e['time_us'] = e['time_us_abs'] - t0

    return gpio_events, True


def cross_reference(uart_events, gpio_events):
    rows = []
    match = True

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

    plot_trace(axes[0], uart_events, 'time_us',
               lambda e: 1 if e['state'] == 'LED ON' else 0,
               'Firmware-reported (UART printf)', '#f1c40f')

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


# ── Report data assembly ─────────────────────────────────────────────

def build_report_data(uart_file, checks, uart_events, gpio_events,
                      xref_rows, xref_match, gpio_log_found, waveform_png):
    with open(uart_file) as f:
        raw_uart = f.read().strip()

    with open(waveform_png, 'rb') as f:
        waveform_b64 = base64.b64encode(f.read()).decode()

    check_list = [
        {'name': 'Boot message received',      'passed': checks['boot']},
        {'name': 'UART initialized',           'passed': checks['uart_init']},
        {'name': 'LED cycles (5 expected)',     'passed': checks['led_cycles'] == 5},
        {'name': 'Test completion message',     'passed': checks['test_complete']},
    ]
    if gpio_log_found:
        check_list.append({'name': 'GPIO transitions match UART claims', 'passed': xref_match})
        check_list.append({'name': f'Emulator GPIO events ({len(gpio_events)} observed)',
                           'passed': len(gpio_events) == 10})

    all_pass = all(c['passed'] for c in check_list)

    return {
        'generated_at': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'all_pass': all_pass,
        'checks': check_list,
        'waveform_b64': waveform_b64,
        'gpio_log_found': gpio_log_found,
        'xref_rows': xref_rows,
        'uart_events': uart_events,
        'raw_uart': raw_uart,
    }


def generate_html(report_data, template_path, output_path):
    with open(template_path) as f:
        template = f.read()

    data_json = json.dumps(report_data, indent=2)
    injected = template.replace(
        '// __REPORT_DATA_PLACEHOLDER__',
        f'window.__REPORT_DATA__ = {data_json};'
    )

    with open(output_path, 'w') as f:
        f.write(injected)
    print(f"[report] HTML report saved → {output_path}")
    return report_data['all_pass']


# ── main ────────────────────────────────────────────────────────────

if __name__ == '__main__':
    output_dir    = sys.argv[1] if len(sys.argv) > 1 else '/workspace/output'
    uart_file     = os.path.join(output_dir, 'uart_output.txt')
    renode_log    = os.path.join(output_dir, 'renode.log')
    waveform_png  = os.path.join(output_dir, 'waveform.png')
    report_html   = os.path.join(output_dir, 'report.html')
    template_path = os.path.join(os.path.dirname(__file__), 'report_template.html')

    if not os.path.exists(uart_file):
        print(f"[report] ERROR: {uart_file} not found")
        sys.exit(1)

    uart_events, checks = parse_uart_log(uart_file)
    gpio_events, gpio_log_found = parse_renode_log(renode_log)

    if gpio_log_found:
        print(f"[report] Parsed {len(gpio_events)} GPIO transitions from Renode log")
    else:
        print(f"[report] No Renode log found — GPIO verification unavailable")

    xref_rows, xref_match = cross_reference(uart_events, gpio_events)

    ok = generate_waveform(uart_events, gpio_events, waveform_png)
    if ok:
        report_data = build_report_data(
            uart_file, checks, uart_events, gpio_events,
            xref_rows, xref_match, gpio_log_found, waveform_png)
        all_pass = generate_html(report_data, template_path, report_html)
        sys.exit(0 if all_pass else 1)
    else:
        sys.exit(1)
