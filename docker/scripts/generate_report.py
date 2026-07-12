#!/usr/bin/env python3
"""Render validation.json as the IDE's self-contained HTML report."""

from __future__ import annotations

import html
import json
import sys
from pathlib import Path


def main() -> int:
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "/workspace/output")
    result_path = output / "validation.json"
    if not result_path.exists():
        return 1
    result = json.loads(result_path.read_text(encoding="utf-8"))
    cards = "".join(
        f'<div class="card"><b>{html.escape(label)}</b><span>{value}</span></div>'
        for label, value in [
            ("Authorized tags", f"{result['seen_tags']} / {result['tag_count']}"),
            ("Rotating tags", f"{result['rotated_tags']} / {result['tag_count']}"),
            ("Valid sightings", result["valid_packets"]),
            ("Rogues rejected", result["rogue_packets"]),
        ]
    )
    checks = "".join(
        '<li class="%s"><span>%s</span><small>%s</small></li>'
        % (
            "pass" if check["passed"] else "fail",
            html.escape(check["label"]),
            html.escape(check["detail"]),
        )
        for check in result["checks"]
    )
    state = "MISSION SURVIVED" if result["passed"] else "CITY WENT DARK"
    state_class = "ok" if result["passed"] else "bad"
    document = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GhostTag Apocalypse report</title><style>
:root{{--bg:#08110f;--panel:#10201c;--ink:#d9fbe7;--muted:#83a99a;--acid:#62ff9b;--red:#ff5d73;--line:#26483e}}
*{{box-sizing:border-box}}body{{margin:0;background:radial-gradient(circle at 80% 0,#143d31,var(--bg) 42%);color:var(--ink);font:15px/1.5 ui-monospace,SFMono-Regular,Menlo,monospace}}
main{{max-width:960px;margin:auto;padding:36px 22px}}h1{{font-size:clamp(30px,6vw,64px);line-height:.95;margin:.25em 0}}.eyebrow{{color:var(--acid);letter-spacing:.2em}}.status{{display:inline-block;padding:8px 12px;border:1px solid;border-radius:4px;font-weight:800}}.ok{{color:var(--acid)}}.bad{{color:var(--red)}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin:28px 0}}.card{{background:var(--panel);border:1px solid var(--line);padding:16px;display:grid;gap:10px}}.card b,small{{color:var(--muted)}}.card span{{font-size:24px;color:var(--acid)}}ul{{padding:0;list-style:none}}li{{display:flex;justify-content:space-between;gap:18px;padding:13px 0;border-bottom:1px solid var(--line)}}li:before{{font-weight:900}}li.pass:before{{content:'PASS';color:var(--acid)}}li.fail:before{{content:'FAIL';color:var(--red)}}li span{{flex:1}}footer{{margin-top:28px;color:var(--muted)}}
</style></head><body><main><div class="eyebrow">SECTOR {result['sector']} // OFFLINE FIND MESH</div>
<h1>GHOSTTAG<br>APOCALYPSE</h1><div class="status {state_class}">{state}</div>
<div class="grid">{cards}</div><h2>Survival checks</h2><ul>{checks}</ul>
<footer>nRF52840 BLE machines in Renode · deterministic wireless range · authenticated rotating identities</footer>
</main></body></html>"""
    (output / "report.html").write_text(document, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
