#!/usr/bin/env python3
import html
import os
import re
import sys
from datetime import datetime


def read_text(path):
    if not os.path.exists(path):
        return ""
    with open(path, "r", errors="replace") as f:
        return f.read()


def parse_uart(raw):
    samples = []
    threshold = {}
    model = {}
    disagreements = []

    checks = {
        "boot": "BOOT: RP2040 sensor filter TinyML lab" in raw,
        "sensor_init": "I2C sensor ready addr=0x52" in raw,
        "test_complete": "TEST COMPLETE" in raw,
    }

    for line in raw.splitlines():
        line = line.strip()
        m = re.match(r"SAMPLE seq=(\d+) value=(-?\d+)", line)
        if m:
            samples.append({"seq": int(m.group(1)), "value": int(m.group(2))})
            continue

        m = re.match(r"THRESHOLD seq=(\d+) decision=(KEEP|DROP)", line)
        if m:
            threshold[int(m.group(1))] = m.group(2)
            continue

        m = re.match(r"MODEL seq=(\d+) decision=(KEEP|DROP) score=(-?\d+)", line)
        if m:
            model[int(m.group(1))] = {
                "decision": m.group(2),
                "score": int(m.group(3)),
            }
            continue

        m = re.match(r"DISAGREE seq=(\d+)", line)
        if m:
            disagreements.append(int(m.group(1)))

    checks["sample_count"] = len(samples) == 10
    checks["threshold_high_drop"] = threshold.get(4) == "DROP"
    checks["threshold_low_drop"] = threshold.get(6) == "DROP"
    checks["model_borderline_drop"] = model.get(2, {}).get("decision") == "DROP"
    checks["model_second_borderline_drop"] = (
        model.get(7, {}).get("decision") == "DROP"
    )
    checks["disagreements"] = len(disagreements) == 2
    checks["summary"] = (
        "SUMMARY threshold_keep=8 threshold_drop=2 "
        "model_keep=6 model_drop=4 disagreements=2"
    ) in raw

    rows = []
    for sample in samples:
        seq = sample["seq"]
        rows.append(
            {
                "seq": seq,
                "value": sample["value"],
                "threshold": threshold.get(seq, "-"),
                "model": model.get(seq, {}).get("decision", "-"),
                "score": model.get(seq, {}).get("score", "-"),
                "disagree": seq in disagreements,
            }
        )

    return checks, rows


def write_report(output_dir, checks, rows, raw_uart):
    check_names = [
        ("boot", "Boot message"),
        ("sensor_init", "Virtual I2C sensor initialized"),
        ("sample_count", "All 10 samples read"),
        ("threshold_high_drop", "Threshold rejects high outlier"),
        ("threshold_low_drop", "Threshold rejects low outlier"),
        ("model_borderline_drop", "Model rejects borderline jump"),
        ("model_second_borderline_drop", "Model rejects second borderline jump"),
        ("disagreements", "Two method disagreements observed"),
        ("summary", "Summary counts match expected result"),
        ("test_complete", "Firmware completed run"),
    ]
    all_pass = all(checks.get(key, False) for key, _ in check_names)

    check_rows = []
    for key, label in check_names:
        passed = checks.get(key, False)
        check_rows.append(
            f"<tr><td>{html.escape(label)}</td>"
            f"<td class=\"{'pass' if passed else 'fail'}\">"
            f"{'PASS' if passed else 'FAIL'}</td></tr>"
        )

    sample_rows = []
    for row in rows:
        sample_rows.append(
            "<tr>"
            f"<td>{row['seq']}</td>"
            f"<td>{row['value']}</td>"
            f"<td>{html.escape(str(row['threshold']))}</td>"
            f"<td>{html.escape(str(row['model']))}</td>"
            f"<td>{html.escape(str(row['score']))}</td>"
            f"<td>{'yes' if row['disagree'] else ''}</td>"
            "</tr>"
        )

    page = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>RP2040 Sensor Filter TinyML Report</title>
  <style>
    body {{ margin: 0; background: #101318; color: #e6edf3; font-family: Inter, system-ui, sans-serif; }}
    main {{ max-width: 1120px; margin: 0 auto; padding: 32px; }}
    h1 {{ font-size: 28px; margin: 0 0 8px; }}
    h2 {{ font-size: 18px; margin-top: 28px; }}
    .status {{ display: inline-block; padding: 6px 10px; border-radius: 4px; margin: 8px 0 18px; }}
    .status.pass {{ background: #0f5132; }}
    .status.fail {{ background: #842029; }}
    table {{ border-collapse: collapse; width: 100%; background: #161b22; }}
    th, td {{ border-bottom: 1px solid #30363d; padding: 9px 10px; text-align: left; }}
    th {{ color: #8b949e; font-weight: 600; }}
    .pass {{ color: #7ee787; }}
    .fail {{ color: #ff7b72; }}
    pre {{ background: #0d1117; border: 1px solid #30363d; padding: 14px; overflow: auto; }}
  </style>
</head>
<body>
<main>
  <h1>RP2040 Sensor Filter TinyML Report</h1>
  <div>Generated {html.escape(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))}</div>
  <div class="status {'pass' if all_pass else 'fail'}">
    {html.escape('All checks passed' if all_pass else 'Complete the firmware TODOs')}
  </div>
  <h2>Validation Checks</h2>
  <table>{''.join(check_rows)}</table>
  <h2>Sample Timeline</h2>
  <table>
    <thead><tr><th>Seq</th><th>Value</th><th>Threshold</th><th>Model</th><th>Score</th><th>Disagree</th></tr></thead>
    <tbody>{''.join(sample_rows)}</tbody>
  </table>
  <h2>Raw UART</h2>
  <pre>{html.escape(raw_uart)}</pre>
</main>
</body>
</html>
"""
    report_path = os.path.join(output_dir, "report.html")
    with open(report_path, "w") as f:
        f.write(page)
    print(f"[report] HTML report saved -> {report_path}")
    return all_pass


def main():
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "/workspace/output"
    uart_path = os.path.join(output_dir, "uart_output.txt")
    raw_uart = read_text(uart_path)
    checks, rows = parse_uart(raw_uart)
    return 0 if write_report(output_dir, checks, rows, raw_uart) else 1


if __name__ == "__main__":
    raise SystemExit(main())
