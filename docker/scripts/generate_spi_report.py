#!/usr/bin/env python3
"""
generate_spi_report.py
Parses master/slave UART outputs from the dual-STM32F746 STMP v1 test
and produces a self-contained HTML report at /workspace/output/report.html.
"""

import re, sys, json, base64, os
from datetime import datetime

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import io
    HAS_MPL = True
except ImportError:
    HAS_MPL = False

OUTPUT_DIR  = "/workspace/output"
MASTER_UART = os.path.join(OUTPUT_DIR, "master_uart.txt")
SLAVE_UART  = os.path.join(OUTPUT_DIR, "slave_uart.txt")
RENODE_LOG  = os.path.join(OUTPUT_DIR, "renode.log")
REPORT_PATH = os.path.join(OUTPUT_DIR, "report.html")

EXCHANGE_LABELS = {
    1:  "PING",
    2:  "PING",
    3:  "REG_WR reg=0 val=CAFEBABE",
    4:  "REG_RD reg=0",
    5:  "REG_WR reg=1 val=DEAD1234",
    6:  "REG_RD reg=1",
    7:  "STAT_REQ",
    8:  "PING",
    9:  "REG_RD reg=0 (persist)",
    10: "STAT_REQ",
}

EXPECTED_MOSI = {
    1:  "A5 01 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DD 55",
    2:  "A5 02 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A3 55",
    3:  "A5 03 20 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 D3 55",
    4:  "A5 04 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 98 55",
    5:  "A5 05 20 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 82 55",
    6:  "A5 06 30 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 79 55",
    7:  "A5 07 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 48 55",
    8:  "A5 08 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55",
    9:  "A5 09 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 4D 55",
    10: "A5 0A 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 9D 55",
}

EXPECTED_MISO = {
    1:  "A5 01 11 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 55",
    2:  "A5 02 11 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C0 55",
    3:  "A5 03 21 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55",
    4:  "A5 04 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 EB 55",
    5:  "A5 05 21 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 E9 55",
    6:  "A5 06 31 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 12 55",
    7:  "A5 07 41 00 07 00 00 00 06 00 00 00 00 00 00 00 00 00 00 06 55",
    8:  "A5 08 11 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1D 55",
    9:  "A5 09 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 3E 55",
    10: "A5 0A 41 00 0A 00 00 00 09 00 00 00 00 00 00 00 00 00 00 AA 55",
}


def read_file(path):
    try:
        with open(path) as f:
            return f.read()
    except FileNotFoundError:
        return ""


def parse_master(text):
    r = {}
    r["start"]      = "MASTER: STMP v1 test start" in text
    r["all_passed"] = "MASTER: ALL TESTS PASSED" in text
    r["all_failed"] = "MASTER: TEST FAILED" in text
    r["pkt_ok"]   = {}
    r["pkt_fail"] = {}
    for m in re.finditer(r"MASTER: \[(\d+)\] .+ OK", text):
        r["pkt_ok"][int(m.group(1))] = True
    for m in re.finditer(r"MASTER: \[(\d+)\] .+ FAIL(.*)", text):
        r["pkt_fail"][int(m.group(1))] = m.group(2).strip()
    return r


def parse_slave(text):
    r = {}
    r["start"]    = "SLAVE: STMP v1 ready" in text
    r["all_done"] = "SLAVE: ALL DONE" in text
    r["pkt_ok"]   = {}
    for m in re.finditer(r"SLAVE: \[(\d+)\]", text):
        r["pkt_ok"][int(m.group(1))] = True
    return r


def parse_log(text):
    """Extract actual MOSI/MISO frames from Renode log."""
    actual_mosi, actual_miso = {}, {}
    for m in re.finditer(r"SPI_BUS: MOSI_REQ seq=(\d+) frame=([0-9A-F ]+)", text):
        actual_mosi[int(m.group(1))] = m.group(2).strip()
    for m in re.finditer(r"SPI_BUS: MISO_RSP seq=(\d+) frame=([0-9A-F ]+)", text):
        actual_miso[int(m.group(1))] = m.group(2).strip()
    return actual_mosi, actual_miso


def make_chart(master_data):
    if not HAS_MPL:
        return None
    labels, colors = [], []
    for n in range(1, 11):
        labels.append(f"[{n}] {EXCHANGE_LABELS.get(n, '')}")
        colors.append("#4caf50" if n in master_data["pkt_ok"] else "#f44336")

    fig, ax = plt.subplots(figsize=(10, 3))
    bars = ax.barh(labels, [1]*10, color=colors, edgecolor="white")
    ax.set_xlim(0, 1)
    ax.set_xticks([])
    ax.set_title("STMP v1 Exchange Results (green=PASS, red=FAIL)")
    ax.invert_yaxis()
    plt.tight_layout()
    buf = io.BytesIO()
    plt.savefig(buf, format="png", dpi=100)
    plt.close()
    buf.seek(0)
    return base64.b64encode(buf.read()).decode()


def frame_row(n, actual_mosi, actual_miso):
    exp_mosi = EXPECTED_MOSI.get(n, "")
    exp_miso = EXPECTED_MISO.get(n, "")
    got_mosi = actual_mosi.get(n, "—")
    got_miso = actual_miso.get(n, "—")

    def cell(expected, actual):
        ok = (actual == expected)
        bg = "#e8f5e9" if ok else "#ffebee"
        mark = "&#10003;" if ok else "&#10007;"
        return f'<td style="background:{bg};font-family:monospace;font-size:11px">{mark} {actual}</td>'

    label = EXCHANGE_LABELS.get(n, "")
    return (f'<tr><td style="text-align:center">{n}</td>'
            f'<td>{label}</td>'
            f'{cell(exp_mosi, got_mosi)}'
            f'{cell(exp_miso, got_miso)}</tr>')


def build_report(master, slave, actual_mosi, actual_miso,
                 master_raw, slave_raw, log_raw):
    chart_b64 = make_chart(master)
    passed_count = len(master["pkt_ok"])
    failed_count = len(master["pkt_fail"])
    overall = master["all_passed"] and slave["all_done"]
    badge_bg = "#4caf50" if overall else "#f44336"
    badge_text = "ALL PASS" if overall else "FAIL"

    rows = "\n".join(frame_row(n, actual_mosi, actual_miso) for n in range(1, 11))

    chart_html = (f'<img src="data:image/png;base64,{chart_b64}" '
                  f'style="max-width:100%;margin:12px 0"/>'
                  if chart_b64 else "<p><em>matplotlib not available</em></p>")

    summary_data = {
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "master_start": master["start"],
        "master_all_passed": master["all_passed"],
        "slave_start": slave["start"],
        "slave_all_done": slave["all_done"],
        "exchanges_passed": passed_count,
        "exchanges_failed": failed_count,
        "mosi_frames_verified": sum(
            1 for n in range(1, 11)
            if actual_mosi.get(n) == EXPECTED_MOSI.get(n)),
        "miso_frames_verified": sum(
            1 for n in range(1, 11)
            if actual_miso.get(n) == EXPECTED_MISO.get(n)),
    }

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<title>STMP v1 SPI Test Report</title>
<style>
  body{{font-family:sans-serif;max-width:1100px;margin:20px auto;background:#fafafa}}
  h1{{color:#333}}
  .badge{{display:inline-block;padding:6px 18px;border-radius:4px;
          color:#fff;font-weight:bold;font-size:1.2em;background:{badge_bg}}}
  table{{border-collapse:collapse;width:100%;margin-top:12px}}
  th,td{{border:1px solid #ddd;padding:6px 10px;vertical-align:top}}
  th{{background:#eeeeee}}
  pre{{background:#f5f5f5;padding:10px;overflow-x:auto;font-size:12px}}
  details{{margin:8px 0}}
</style>
</head>
<body>
<h1>STMP v1 — Dual STM32F746 SPI Protocol Test Report</h1>
<p>Generated: {datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')} UTC</p>
<p><span class="badge">{badge_text}</span>
   &nbsp; Exchanges: {passed_count} passed / {failed_count} failed</p>

{chart_html}

<h2>Per-Exchange Frame Verification</h2>
<p>MOSI = master&rarr;slave request; MISO = slave&rarr;master response.
   Frames are checked byte-for-byte against CRC-8/SMBUS-validated expected values.</p>
<table>
<thead><tr>
  <th>#</th><th>Command</th>
  <th>MOSI (actual)</th><th>MISO (actual)</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>

<h2>UART Output</h2>
<details open><summary>Master UART</summary><pre>{master_raw or "(empty)"}</pre></details>
<details open><summary>Slave UART</summary><pre>{slave_raw or "(empty)"}</pre></details>

<h2>Emulation Log (excerpt)</h2>
<details><summary>Renode SPI bus log (SPI_BUS / SPI_LINE lines)</summary>
<pre>{chr(10).join(l for l in log_raw.splitlines() if "SPI_BUS" in l or "SPI_LINE" in l) or "(no log)"}</pre>
</details>

<h2>Summary JSON</h2>
<pre>{json.dumps(summary_data, indent=2)}</pre>
</body>
</html>"""


def main():
    master_raw = read_file(MASTER_UART)
    slave_raw  = read_file(SLAVE_UART)
    log_raw    = read_file(RENODE_LOG)

    master = parse_master(master_raw)
    slave  = parse_slave(slave_raw)
    actual_mosi, actual_miso = parse_log(log_raw)

    html = build_report(master, slave, actual_mosi, actual_miso,
                        master_raw, slave_raw, log_raw)
    with open(REPORT_PATH, "w") as f:
        f.write(html)
    print(f"Report written to {REPORT_PATH}")

    total_checks = 10
    passed = sum(1 for n in range(1, 11) if n in master["pkt_ok"])
    print(f"Checks: {passed}/{total_checks} passed")


if __name__ == "__main__":
    main()
