#!/usr/bin/env python3
"""
Generates a self-contained HTML report for the RP2040 dual-machine
I2C humidity / LoRaWAN digital-twin test.
"""
import base64
import json
import os
import re
import sys
from datetime import datetime

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_node_uart(filepath):
    events = []
    checks = {
        "node_boot": False,
        "node_sensor": False,
        "node_temperature": False,
        "node_lora": False,
    }
    if not os.path.exists(filepath):
        return events, checks, ""

    with open(filepath) as f:
        raw = f.read()

    for line in raw.strip().splitlines():
        line = line.strip()
        if not line:
            continue
        if "[NODE] Boot:" in line:
            checks["node_boot"] = True
            events.append({"type": "boot", "machine": "node", "message": line})
        if "[NODE] HDC1080:" in line:
            checks["node_sensor"] = True
            events.append({"type": "sensor", "machine": "node", "message": line})
            if "temp=" in line:
                checks["node_temperature"] = True
        if "[NODE] LoRa TX:" in line:
            checks["node_lora"] = True
            events.append({"type": "lora_tx", "machine": "node", "message": line})

    return events, checks, raw.strip()


def parse_master_uart(filepath):
    events = []
    checks = {
        "master_boot": False,
        "master_poll": False,
        "master_humidity": False,
        "master_temperature": False,
    }
    humidity_readings = []

    if not os.path.exists(filepath):
        return events, checks, humidity_readings, ""

    with open(filepath) as f:
        raw = f.read()

    for line in raw.strip().splitlines():
        line = line.strip()
        if not line:
            continue
        if "[MASTER] Boot:" in line:
            checks["master_boot"] = True
            events.append({"type": "boot", "machine": "master", "message": line})
        if "[MASTER] Poll #" in line:
            checks["master_poll"] = True
            events.append({"type": "poll", "machine": "master", "message": line})
            poll_match = re.search(r"Poll #(\d+)", line)
            poll_num = int(poll_match.group(1)) if poll_match else 0
            hum_match = re.search(r"humidity=(\d+(?:\.\d+)?)", line)
            if hum_match:
                checks["master_humidity"] = True
                humidity_readings.append(
                    {"poll": poll_num, "humidity": float(hum_match.group(1))}
                )
            temp_match = re.search(r"temp=([\-]?\d+(?:\.\d+)?)", line)
            if temp_match:
                checks["master_temperature"] = True
        if "humidity=" in line and "[MASTER]" in line:
            checks["master_humidity"] = True

    return events, checks, humidity_readings, raw.strip()


def generate_humidity_chart(humidity_readings, output_path):
    if not humidity_readings:
        print("[report] No humidity readings -- skipping chart")
        return False

    fig, ax = plt.subplots(1, 1, figsize=(13, 3.5))
    fig.patch.set_facecolor("#0f0f23")
    ax.set_facecolor("#0f0f23")

    polls = [r["poll"] for r in humidity_readings]
    humidities = [r["humidity"] for r in humidity_readings]
    ax.plot(
        polls,
        humidities,
        color="#00d4ff",
        linewidth=2,
        marker="o",
        markersize=5,
        markerfacecolor="#00d4ff",
        markeredgecolor="#0f0f23",
    )
    ax.fill_between(polls, humidities, alpha=0.15, color="#00d4ff")
    ax.set_xlabel("Poll Cycle", color="#a0a0c0", fontsize=10)
    ax.set_ylabel("Humidity (%)", color="#a0a0c0", fontsize=10)
    ax.set_title(
        "Master Humidity Readings Over Time",
        color="#00d4ff",
        fontsize=11,
        pad=8,
        loc="left",
    )
    ax.tick_params(colors="#a0a0c0")

    for spine in ax.spines.values():
        spine.set_edgecolor("#333355")
    ax.grid(True, color="#333355", linewidth=0.5, alpha=0.5)

    if humidities:
        ax.set_ylim(max(0, min(humidities) - 5), min(100, max(humidities) + 5))

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor=fig.get_facecolor())
    plt.close()
    print(f"[report] Humidity chart saved -> {output_path}")
    return True


def build_report_data(
    node_checks,
    master_checks,
    node_events,
    master_events,
    humidity_readings,
    node_raw,
    master_raw,
    chart_b64,
):
    check_list = [
        {"name": "Node boot message", "passed": node_checks["node_boot"]},
        {"name": "Node HDC1080 sensor read", "passed": node_checks["node_sensor"]},
        {
            "name": "Node temperature sensor read",
            "passed": node_checks["node_temperature"],
        },
        {"name": "Node LoRa TX", "passed": node_checks["node_lora"]},
        {"name": "Master boot message", "passed": master_checks["master_boot"]},
        {"name": "Master poll cycle", "passed": master_checks["master_poll"]},
        {
            "name": "Master humidity reading",
            "passed": master_checks["master_humidity"],
        },
        {
            "name": "Master temperature reading",
            "passed": master_checks["master_temperature"],
        },
    ]
    return {
        "generated_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "all_pass": all(c["passed"] for c in check_list),
        "checks": check_list,
        "chart_b64": chart_b64,
        "node_events": node_events,
        "master_events": master_events,
        "humidity_readings": humidity_readings,
        "node_raw_uart": node_raw,
        "master_raw_uart": master_raw,
    }


def generate_html(report_data, template_path, output_path):
    with open(template_path) as f:
        template = f.read()
    data_json = json.dumps(report_data, indent=2)
    injected = template.replace(
        "// __REPORT_DATA_PLACEHOLDER__",
        f"window.__REPORT_DATA__ = {data_json};",
    )
    with open(output_path, "w") as f:
        f.write(injected)
    print(f"[report] HTML report saved -> {output_path}")
    return report_data["all_pass"]


if __name__ == "__main__":
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "/workspace/output"
    node_uart = os.path.join(output_dir, "node_uart.txt")
    master_uart = os.path.join(output_dir, "master_uart.txt")
    chart_png = os.path.join(output_dir, "humidity_chart.png")
    report_html = os.path.join(output_dir, "report.html")
    template_path = os.path.join(os.path.dirname(__file__), "report_template.html")

    if not os.path.exists(node_uart):
        print(f"[report] WARNING: {node_uart} not found")
    node_events, node_checks, node_raw = parse_node_uart(node_uart)
    print(f"[report] Node: {len(node_events)} events parsed")

    if not os.path.exists(master_uart):
        print(f"[report] WARNING: {master_uart} not found")
    master_events, master_checks, humidity_readings, master_raw = parse_master_uart(
        master_uart
    )
    print(
        f"[report] Master: {len(master_events)} events, "
        f"{len(humidity_readings)} humidity readings"
    )

    chart_b64 = ""
    if generate_humidity_chart(humidity_readings, chart_png):
        with open(chart_png, "rb") as f:
            chart_b64 = base64.b64encode(f.read()).decode()

    report_data = build_report_data(
        node_checks,
        master_checks,
        node_events,
        master_events,
        humidity_readings,
        node_raw,
        master_raw,
        chart_b64,
    )
    all_pass = generate_html(report_data, template_path, report_html)
    sys.exit(0 if all_pass else 1)
