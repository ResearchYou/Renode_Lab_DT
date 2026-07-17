#!/usr/bin/env python3
"""Validate gateway evidence and write a machine-readable result."""

from __future__ import annotations

import json
import os
import re
from pathlib import Path


OUTPUT = Path(os.environ.get("OUTPUT_DIR", "/workspace/output"))
TAG_COUNT = int(os.environ.get("TAG_COUNT", "6"))
GATEWAY_COUNT = int(os.environ.get("GATEWAY_COUNT", "3"))
SECTOR = int(os.environ.get("SECTOR_INDEX", "0"))
TAG_BASE = SECTOR * TAG_COUNT

SIGHT = re.compile(
    r"GHOST_SIGHT gateway=(?P<gateway>\d+) tag=(?P<tag>\d+) "
    r"sector=(?P<sector>\d+) epoch=(?P<epoch>\d+) "
    r"eid=(?P<eid>[0-9a-f]+)(?: .*rotated=(?P<rotated>[01]))?"
)
SUMMARY = re.compile(
    r"GHOST_SUMMARY gateway=(?P<gateway>\d+) sector=(?P<sector>\d+) "
    r"unique=(?P<unique>\d+) valid=(?P<valid>\d+) "
    r"rotations=(?P<rotations>\d+) rogues=(?P<rogues>\d+) "
    r"replays=(?P<replays>\d+)"
)
STATE = re.compile(
    r"GHOST_STATE_READY epoch=(?P<epoch>\d+) lease_end=(?P<lease>\d+) "
    r"generation=(?P<generation>\d+) recovered=(?P<recovered>[01])"
)
ROTATE = re.compile(r"GHOST_ROTATE epoch=(?P<epoch>\d+)")
ENERGY = re.compile(
    r"GHOST_ENERGY epoch=(?P<epoch>\d+) advertisements=(?P<advertisements>\d+) "
    r"writes=(?P<writes>\d+) erases=(?P<erases>\d+) units=(?P<units>\d+)"
)


def add_check(checks: list[dict], label: str, passed: bool, detail: str) -> None:
    checks.append({"label": label, "passed": passed, "detail": detail})
    state = "PASS" if passed else "FAIL"
    print(f"[{state}] {label}: {detail}")


def main() -> int:
    checks: list[dict] = []
    protocol_status_path = OUTPUT / "protocol-tests.status"
    protocol_status = (
        int(protocol_status_path.read_text().strip())
        if protocol_status_path.exists()
        else 127
    )
    add_check(
        checks,
        "protocol, persistence, and endurance suite",
        protocol_status == 0,
        f"exit={protocol_status}",
    )

    seen_tags: set[int] = set()
    rotated_tags: set[int] = set()
    ready_gateways: set[int] = set()
    summaries: dict[int, dict[str, int]] = {}
    fatal_lines: list[str] = []

    for gateway in range(GATEWAY_COUNT):
        path = OUTPUT / f"gateway-{gateway}.log"
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if "GHOST_GATEWAY_READY" in line:
                ready_gateways.add(gateway)
            if "FATAL" in line:
                fatal_lines.append(line)
            match = SIGHT.search(line)
            if match:
                tag = int(match.group("tag"))
                seen_tags.add(tag)
                if match.group("rotated") == "1":
                    rotated_tags.add(tag)
            match = SUMMARY.search(line)
            if match:
                summaries[gateway] = {
                    key: int(match.group(key))
                    for key in (
                        "unique",
                        "valid",
                        "rotations",
                        "rogues",
                        "replays",
                    )
                }

    expected_tags = set(range(TAG_BASE + 1, TAG_BASE + TAG_COUNT + 1))
    missing = sorted(expected_tags - seen_tags)
    not_rotated = sorted(expected_tags - rotated_tags)
    rogue_total = sum(item["rogues"] for item in summaries.values())
    replay_total = sum(item["replays"] for item in summaries.values())
    valid_total = sum(item["valid"] for item in summaries.values())

    tag_log = OUTPUT / "tag-sample.log"
    tag_lines = (
        tag_log.read_text(encoding="utf-8", errors="replace").splitlines()
        if tag_log.exists()
        else []
    )
    state_events = [STATE.search(line) for line in tag_lines]
    state_events = [match for match in state_events if match]
    emitted_epochs = [int(match.group("epoch")) for match in state_events]
    emitted_epochs.extend(
        int(match.group("epoch"))
        for line in tag_lines
        if (match := ROTATE.search(line))
    )
    recovered_epochs = [
        int(match.group("epoch"))
        for match in state_events
        if match.group("recovered") == "1"
    ]
    energy_events = [ENERGY.search(line) for line in tag_lines]
    energy_events = [match for match in energy_events if match]
    max_energy = max(
        (int(match.group("units")) for match in energy_events), default=10**9
    )
    monotonic_after_cut = (
        bool(recovered_epochs)
        and max(recovered_epochs) >= 16
        and len(emitted_epochs) == len(set(emitted_epochs))
    )

    add_check(
        checks,
        "all observer gateways boot",
        len(ready_gateways) == GATEWAY_COUNT,
        f"ready={len(ready_gateways)}/{GATEWAY_COUNT}",
    )
    add_check(
        checks,
        "entire authorized fleet is discoverable",
        not missing,
        "all tags seen" if not missing else f"missing={missing}",
    )
    add_check(
        checks,
        "every stable identity rotates on air",
        not not_rotated,
        "all tags rotated" if not not_rotated else f"not_rotated={not_rotated}",
    )
    add_check(
        checks,
        "unregistered clone traffic is rejected",
        rogue_total > 0,
        f"rogue_packets={rogue_total}",
    )
    add_check(
        checks,
        "captured authorized traffic is rejected as replay",
        replay_total > 0,
        f"replay_packets={replay_total}",
    )
    add_check(
        checks,
        "power-cut recovery never reuses a ratchet epoch",
        monotonic_after_cut,
        (
            f"recovered_epochs={recovered_epochs} unique_epochs={len(set(emitted_epochs))}"
            if state_events
            else "no persistent state evidence"
        ),
    )
    add_check(
        checks,
        "flash and advertising energy stays within budget",
        max_energy <= 80,
        f"units={max_energy}/80",
    )
    add_check(
        checks,
        "no firmware fatal errors",
        not fatal_lines,
        "none" if not fatal_lines else "; ".join(fatal_lines[:3]),
    )

    result = {
        "scenario": "GhostTag Apocalypse",
        "sector": SECTOR,
        "tag_count": TAG_COUNT,
        "gateway_count": GATEWAY_COUNT,
        "seen_tags": len(expected_tags & seen_tags),
        "rotated_tags": len(expected_tags & rotated_tags),
        "valid_packets": valid_total,
        "rogue_packets": rogue_total,
        "replay_packets": replay_total,
        "power_cut_recovered": monotonic_after_cut,
        "energy_units": max_energy,
        "checks": checks,
        "passed": all(check["passed"] for check in checks),
    }
    (OUTPUT / "validation.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(
        "GHOST_VALIDATION "
        + " ".join(
            [
                f"passed={int(result['passed'])}",
                f"sector={SECTOR}",
                f"tags={result['seen_tags']}/{TAG_COUNT}",
                f"rotated={result['rotated_tags']}/{TAG_COUNT}",
                f"valid={valid_total}",
                f"rogues={rogue_total}",
                f"replays={replay_total}",
                f"recovered={int(monotonic_after_cut)}",
                f"energy={max_energy}/80",
            ]
        )
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
