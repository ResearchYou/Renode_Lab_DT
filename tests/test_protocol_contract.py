from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "reference/firmware/ghost_protocol.c"
STARTER = ROOT / "firmware/ghost_protocol.c"
HEADER_DIR = ROOT / "firmware/include"
CONTRACT_TEST = ROOT / "support/tests/test_protocol_contract.c"


def compile_and_run(source: str) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="ghosttag-contract-") as tmp:
        source_path = Path(tmp) / "ghost_protocol.c"
        binary_path = Path(tmp) / "contract-tests"
        source_path.write_text(source, encoding="utf-8")
        compiled = subprocess.run(
            [
                "gcc",
                "-std=c17",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(HEADER_DIR),
                str(source_path),
                str(CONTRACT_TEST),
                "-o",
                str(binary_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        return subprocess.run(
            [str(binary_path)],
            check=False,
            capture_output=True,
            text=True,
        )


def mutate(source: str, old: str, new: str) -> str:
    assert source.count(old) == 1, f"mutation target count for {old!r} was not one"
    return source.replace(old, new, 1)


def test_reference_implementation_passes_contract() -> None:
    result = compile_and_run(REFERENCE.read_text(encoding="utf-8"))
    assert result.returncode == 0, result.stdout + result.stderr
    assert "PROTOCOL_CONTRACT_TESTS failures=0" in result.stdout


def test_starter_implementation_fails_contract() -> None:
    result = compile_and_run(STARTER.read_text(encoding="utf-8"))
    assert result.returncode != 0
    assert "[FAIL]" in result.stdout


MUTATIONS = [
    (
        "broken_siphash",
        "return v0 ^ v1 ^ v2 ^ v3;",
        "return 0u;",
    ),
    (
        "wrong_ratchet_domain",
        "input[0] = 0x52u;",
        "input[0] = 0x53u;",
    ),
    (
        "ratchet_lanes_collapsed",
        "input[5] = 1u;",
        "input[5] = 0u;",
    ),
    (
        "wrong_eid_domain",
        "eid_input[0] = 0x45u;",
        "eid_input[0] = 0x46u;",
    ),
    (
        "epoch_not_bound_to_eid",
        "write_u32_le(eid_input + 1, epoch);",
        "write_u32_le(eid_input + 1, 0u);",
    ),
    (
        "raw_key_reused_for_mac",
        """static void mac_key(const uint8_t key[16], uint8_t out[16])
{
    memcpy(out, key, 16);
    out[0] ^= 0x4du;
    out[7] ^= 0x41u;
    out[8] ^= 0x43u;
    out[15] ^= 0xa7u;
}""",
        """static void mac_key(const uint8_t key[16], uint8_t out[16])
{
    memcpy(out, key, 16);
}""",
    ),
    (
        "constant_eid",
        """write_u64_le(out + 12,
                 ghost_siphash24(epoch_key, eid_input, sizeof(eid_input)));""",
        "write_u64_le(out + 12, 0u);",
    ),
    (
        "header_validation_removed",
        """    if (payload[0] != (uint8_t)GHOST_COMPANY_ID ||
        payload[1] != (uint8_t)(GHOST_COMPANY_ID >> 8) ||
        payload[2] != GHOST_PROTOCOL_VERSION || epoch > GHOST_MAX_VERIFY_EPOCH) {
        return false;
    }

""",
        "",
    ),
    (
        "eid_not_verified",
        "for (size_t index = 12; index < GHOST_PAYLOAD_SIZE; ++index)",
        "for (size_t index = 20; index < GHOST_PAYLOAD_SIZE; ++index)",
    ),
    (
        "mac_not_verified",
        "for (size_t index = 12; index < GHOST_PAYLOAD_SIZE; ++index)",
        "for (size_t index = 12; index < 20u; ++index)",
    ),
    (
        "all_payloads_accepted",
        "return difference == 0u;",
        "(void)difference;\n    return true;",
    ),
    (
        "lease_reduced_to_one_epoch",
        "index < GHOST_RATCHET_LEASE_EPOCHS",
        "index < 1u",
    ),
    (
        "crc_validation_disabled",
        "read_u32_le(raw + 32) != crc32(raw, 32u)",
        "(read_u32_le(raw + 32) == UINT32_MAX && crc32(raw, 32u) == UINT32_MAX)",
    ),
    (
        "commit_written_at_wrong_offset",
        "target + JOURNAL_COMMIT_OFFSET, commit,",
        "target + JOURNAL_COMMIT_OFFSET - 1u, commit,",
    ),
    (
        "uncertain_record_skips_only_one_lease",
        "index < 2u * GHOST_RATCHET_LEASE_EPOCHS",
        "index < GHOST_RATCHET_LEASE_EPOCHS",
    ),
    (
        "recovery_flag_not_set",
        """        memcpy(state->epoch_key, best.epoch_key, sizeof(state->epoch_key));
        state->recovered = true;""",
        """        memcpy(state->epoch_key, best.epoch_key, sizeof(state->epoch_key));
        state->recovered = false;""",
    ),
]


@pytest.mark.parametrize(("name", "old", "new"), MUTATIONS, ids=[m[0] for m in MUTATIONS])
def test_invalid_implementations_are_rejected(name: str, old: str, new: str) -> None:
    reference = REFERENCE.read_text(encoding="utf-8")
    result = compile_and_run(mutate(reference, old, new))
    assert result.returncode != 0, f"invalid implementation passed: {name}"
    assert "[FAIL]" in result.stdout
