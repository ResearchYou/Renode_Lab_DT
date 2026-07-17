#!/usr/bin/env python3
"""Organizer-only deterministic GhostTag provisioning helpers."""


def seed_for_tag(tag_id: int) -> int:
    mask = (1 << 64) - 1
    value = tag_id ^ 0x47484F5354544147
    value = (value + 0x9E3779B97F4A7C15) & mask
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & mask
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & mask
    return (value ^ (value >> 31)) & mask
