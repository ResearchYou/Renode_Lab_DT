from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PUBLIC_GENERATOR = ROOT / "docker/scripts/generate_swarm_resc.py"
PRIVATE_SECRETS = ROOT / "docker/scripts/ghosttag_secrets.py"
DOCKERFILE = ROOT / "docker/Dockerfile"


def test_problem_statement_matches_normal_run_scale() -> None:
    problem = (ROOT / "ide/problem/problem.md").read_text(encoding="utf-8")
    assert "normal IDE run uses 6 tags" in problem
    assert "normal IDE run uses 12 tags" not in problem


def test_public_generator_does_not_expose_seed_derivation() -> None:
    public = PUBLIC_GENERATOR.read_text(encoding="utf-8")
    private = PRIVATE_SECRETS.read_text(encoding="utf-8")

    assert "from ghosttag_secrets import seed_for_tag" in public
    assert "def seed_for_tag" not in public
    assert "def seed_for_tag" in private
    for secret_marker in (
        "0x47484F5354544147",
        "0x9E3779B97F4A7C15",
        "0xBF58476D1CE4E5B9",
        "0x94D049BB133111EB",
    ):
        assert secret_marker not in public
        assert secret_marker in private


def test_generator_still_creates_a_complete_scenario() -> None:
    with tempfile.TemporaryDirectory(prefix="ghosttag-resc-") as tmp:
        output = Path(tmp) / "ghosttag.resc"
        env = os.environ.copy()
        env.update(
            {
                "PYTHONPATH": str(ROOT / "docker/scripts"),
                "RESC_OUTPUT": str(output),
                "TAG_COUNT": "6",
                "GATEWAY_COUNT": "3",
                "ATTACKER_COUNT": "2",
                "SECTOR_INDEX": "7",
            }
        )
        result = subprocess.run(
            [sys.executable, str(PUBLIC_GENERATOR)],
            check=False,
            capture_output=True,
            text=True,
            env=env,
        )

        assert result.returncode == 0, result.stdout + result.stderr
        scenario = output.read_text(encoding="utf-8")
        assert scenario.count('mach create "gateway-') == 3
        assert scenario.count('mach create "tag-') == 6
        assert scenario.count('mach create "rogue-') == 2
        assert 'emulation CreateBLEMedium "ghostAir"' in scenario
        assert 'emulation RunFor "00:00:06"' in scenario


def test_participant_image_seeds_exact_explorer_surface() -> None:
    dockerfile = DOCKERFILE.read_text(encoding="utf-8")
    assert "COPY firmware/ /workspace/firmware_seed/" in dockerfile
    assert "COPY ide/problem/ /workspace/problem_seed/" in dockerfile
    assert "ide/renode/README.md /workspace/renode/README.md" in dockerfile
    assert "docker/scripts/generate_swarm_resc.py /workspace/renode/generate_swarm_resc.py" in dockerfile
    assert "docker/scripts/run_test.sh /workspace/renode/run_test.sh" in dockerfile
    assert "platforms/cpus/nrf52840.repl /workspace/renode/nrf52840.repl" in dockerfile
