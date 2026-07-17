#!/usr/bin/env python3
"""Generate a deterministic multi-machine Renode BLE scenario."""

from __future__ import annotations

import math
import os
from pathlib import Path


CONFIG_BASE = 0x000FF000
CONFIG_MAGIC = 0x47484F53


def env_int(name: str, default: int, minimum: int, maximum: int) -> int:
    raw = os.environ.get(name, str(default))
    try:
        value = int(raw)
    except ValueError as exc:
        raise SystemExit(f"{name} must be an integer, got {raw!r}") from exc
    if not minimum <= value <= maximum:
        raise SystemExit(f"{name} must be in [{minimum}, {maximum}], got {value}")
    return value


def seed_for_tag(tag_id: int) -> int:
    mask = (1 << 64) - 1
    value = tag_id ^ 0x47484F5354544147
    value = (value + 0x9E3779B97F4A7C15) & mask
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & mask
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & mask
    return (value ^ (value >> 31)) & mask


def machine(
    name: str,
    elf: str,
    position: tuple[float, float, float],
    address_id: int,
    seed: int,
    sector: int,
    role_id: int,
    uart_path: str | None = None,
) -> list[str]:
    x, y, z = position
    address_low = 0xC0000000 | (address_id & 0x0FFFFFFF)
    address_high = 0x0000D00D | ((address_id & 0xFF) << 16)
    lines = [
        f'mach create "{name}"',
        "machine LoadPlatformDescription @platforms/cpus/nrf52840.repl",
        f"sysbus LoadELF @{elf}",
        f"sysbus WriteDoubleWord 0x100000A0 0x1",
        f"sysbus WriteDoubleWord 0x100000A4 0x{address_low:08X}",
        f"sysbus WriteDoubleWord 0x100000A8 0x{address_high:08X}",
        f"sysbus WriteDoubleWord 0x{CONFIG_BASE:08X} 0x{CONFIG_MAGIC:08X}",
        f"sysbus WriteDoubleWord 0x{CONFIG_BASE + 4:08X} 0x{seed & 0xFFFFFFFF:08X}",
        f"sysbus WriteDoubleWord 0x{CONFIG_BASE + 8:08X} 0x{seed >> 32:08X}",
        f"sysbus WriteDoubleWord 0x{CONFIG_BASE + 12:08X} 0x{sector:08X}",
        f"sysbus WriteDoubleWord 0x{CONFIG_BASE + 16:08X} 0x{role_id:08X}",
        "connector Connect radio ghostAir",
        f"ghostAir SetPosition radio {x:.2f} {y:.2f} {z:.2f}",
    ]
    if uart_path:
        lines.append(f"sysbus.uart0 CreateFileBackend @{uart_path} true")
    lines.extend(["mach clear", ""])
    return lines


def tag_positions(count: int) -> list[tuple[float, float, float]]:
    columns = max(2, math.ceil(math.sqrt(count * 4 / 3)))
    rows = math.ceil(count / columns)
    result = []
    for index in range(count):
        column = index % columns
        row = index // columns
        x = 10.0 + (100.0 * column / max(1, columns - 1))
        y = 10.0 + (78.0 * row / max(1, rows - 1))
        z = float((index * 7) % 9)
        result.append((x, y, z))
    return result


def main() -> None:
    tag_count = env_int("TAG_COUNT", 6, 1, 64)
    gateway_count = env_int("GATEWAY_COUNT", 3, 1, 3)
    attacker_count = env_int("ATTACKER_COUNT", 2, 1, 8)
    sector = env_int("SECTOR_INDEX", 0, 0, 9999)
    duration = env_int("SIMULATION_SECONDS", 8, 4, 60)
    output = Path(os.environ.get("RESC_OUTPUT", "/workspace/build/ghosttag.resc"))
    output.parent.mkdir(parents=True, exist_ok=True)

    tag_id_base = sector * tag_count
    gateway_positions = [(0.0, 0.0, 8.0), (120.0, 0.0, 8.0), (60.0, 104.0, 8.0)]
    lines = [
        ":name: GhostTag Apocalypse generated swarm",
        "using sysbus",
        f"emulation SetSeed {0x106000 + sector}",
        'emulation CreateBLEMedium "ghostAir"',
        "ghostAir SetRangeWirelessFunction 92",
        'emulation SetGlobalQuantum "0.00001"',
        "",
    ]

    for gateway in range(gateway_count):
        lines.extend(
            machine(
                f"gateway-{gateway}",
                "/workspace/build/gateway/zephyr/zephyr.elf",
                gateway_positions[gateway],
                0xF00000 + sector * 16 + gateway,
                0,
                sector,
                gateway,
                f"/workspace/output/gateway-{gateway}.log",
            )
        )

    for local_index, position in enumerate(tag_positions(tag_count)):
        tag_id = tag_id_base + local_index + 1
        lines.extend(
            machine(
                f"tag-{tag_id}",
                "/workspace/build/tag/zephyr/zephyr.elf",
                position,
                tag_id,
                seed_for_tag(tag_id),
                sector,
                tag_id,
                "/workspace/output/tag-sample.log" if local_index == 0 else None,
            )
        )

    for attacker in range(attacker_count):
        rogue_id = tag_id_base + tag_count + 1000 + attacker
        lines.extend(
            machine(
                f"rogue-{attacker}",
                "/workspace/build/tag/zephyr/zephyr.elf",
                (5.0 + attacker * 3.0, 6.0, 1.0),
                rogue_id,
                seed_for_tag(rogue_id),
                sector,
                rogue_id,
            )
        )

    lines.extend(
        [
            'emulation RunFor "00:00:%02d"' % duration,
            "q",
            "",
        ]
    )
    output.write_text("\n".join(lines), encoding="utf-8")
    print(
        f"Generated {output}: sector={sector} tags={tag_count} "
        f"gateways={gateway_count} rogues={attacker_count}"
    )


if __name__ == "__main__":
    main()
