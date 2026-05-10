#!/usr/bin/env python3
import html
import os
import sys
from datetime import datetime


def read_text(path):
    if not os.path.exists(path):
        return ""
    with open(path, "r", errors="replace") as f:
        return f.read()


def parse_events(token_uart, renode_log):
    events = []
    checks = {
        "boot": "TOKEN: boot YK-MOCK challenge-response" in token_uart,
        "get_info": "TOKEN: GET_INFO seq=1 status=OK" in token_uart,
        "auth_first": "TOKEN: AUTH seq=2 touch=1" in token_uart,
        "auth_replay_seen": "TOKEN: AUTH seq=3 touch=1" in token_uart,
        "auth_fresh": "TOKEN: AUTH seq=4 touch=1" in token_uart,
        "script_complete": "TOKEN: SCRIPT COMPLETE" in token_uart,
        "host_get_info": "MOCK_USB_HOST: CHECK GET_INFO OK" in renode_log,
        "host_auth_first": "MOCK_USB_HOST: CHECK AUTH_FIRST OK" in renode_log,
        "host_auth_fresh": "MOCK_USB_HOST: CHECK AUTH_FRESH OK" in renode_log,
        "replay_rejected": "MOCK_USB_HOST: SECURITY_FAIL replay accepted"
        not in renode_log,
    }

    for line in token_uart.splitlines():
        line = line.strip()
        if line.startswith("TOKEN:"):
            kind = "token"
            if "AUTH" in line:
                kind = "auth"
            elif "GET_INFO" in line:
                kind = "info"
            events.append(("UART", kind, line))

    for line in renode_log.splitlines():
        if "MOCK_USB_HOST:" not in line:
            continue
        msg = line.split("MOCK_USB_HOST:", 1)[1].strip()
        kind = "host"
        if "SECURITY_FAIL" in msg:
            kind = "security_fail"
        elif msg.startswith("OUT"):
            kind = "host_out"
        elif msg.startswith("IN"):
            kind = "host_in"
        events.append(("RENODE", kind, "MOCK_USB_HOST: " + msg))

    return events, checks


def render_report(events, checks, token_uart, renode_log, output_path):
    security_pass = checks["replay_rejected"]
    functional_pass = all(
        checks[key]
        for key in (
            "boot",
            "get_info",
            "auth_first",
            "auth_replay_seen",
            "auth_fresh",
            "script_complete",
            "host_get_info",
            "host_auth_first",
            "host_auth_fresh",
        )
    )

    rows = []
    for name, passed in checks.items():
        rows.append(
            f"<tr><td>{html.escape(name)}</td>"
            f"<td class=\"{'pass' if passed else 'fail'}\">"
            f"{'PASS' if passed else 'FAIL'}</td></tr>"
        )

    timeline = []
    for source, kind, message in events:
        timeline.append(
            f"<li class=\"{html.escape(kind)}\"><span>{html.escape(source)}</span>"
            f"{html.escape(message)}</li>"
        )

    all_pass = functional_pass and security_pass
    status_text = "All checks passed"
    if not functional_pass:
        status_text = "Functional validation failed"
    elif not security_pass:
        status_text = "Replay bug reproduced: AUTH replay was accepted"

    page = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>STM32F4 Mock HID Token Report</title>
  <style>
    body {{ margin: 0; background: #101318; color: #e6edf3; font-family: Inter, system-ui, sans-serif; }}
    main {{ max-width: 1120px; margin: 0 auto; padding: 32px; }}
    h1 {{ font-size: 28px; margin: 0 0 8px; }}
    h2 {{ font-size: 18px; margin-top: 28px; }}
    .status {{ display: inline-block; padding: 6px 10px; border-radius: 4px; margin: 8px 0 18px; }}
    .status.pass {{ background: #0f5132; }}
    .status.fail {{ background: #842029; }}
    table {{ border-collapse: collapse; width: 100%; background: #161b22; }}
    td {{ border-bottom: 1px solid #30363d; padding: 9px 10px; }}
    .pass {{ color: #7ee787; }}
    .fail {{ color: #ff7b72; }}
    ol {{ padding-left: 22px; }}
    li {{ margin: 7px 0; line-height: 1.4; }}
    li span {{ color: #8b949e; display: inline-block; min-width: 78px; }}
    pre {{ background: #0d1117; border: 1px solid #30363d; padding: 14px; overflow: auto; }}
  </style>
</head>
<body>
<main>
  <h1>STM32F4 Mock HID Security Token</h1>
  <div>Generated {html.escape(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))}</div>
  <div class="status {'pass' if all_pass else 'fail'}">
    {html.escape(status_text)}
  </div>
  <h2>Validation Checks</h2>
  <table>{''.join(rows)}</table>
  <h2>Timeline</h2>
  <ol>{''.join(timeline)}</ol>
  <h2>Token UART</h2>
  <pre>{html.escape(token_uart)}</pre>
  <h2>Renode Log Excerpt</h2>
  <pre>{html.escape('\\n'.join(line for line in renode_log.splitlines() if 'MOCK_USB_HOST:' in line))}</pre>
</main>
</body>
</html>
"""
    with open(output_path, "w") as f:
        f.write(page)
    return all_pass


def main():
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "/workspace/output"
    token_uart = read_text(os.path.join(output_dir, "token_uart.txt"))
    renode_log = read_text(os.path.join(output_dir, "renode.log"))
    events, checks = parse_events(token_uart, renode_log)
    report_path = os.path.join(output_dir, "report.html")
    all_pass = render_report(events, checks, token_uart, renode_log, report_path)
    print(f"[report] HTML report saved -> {report_path}")
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
