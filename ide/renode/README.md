# Renode scenario files

This directory exposes the public Renode assets used by the immutable challenge
runner:

- `generate_swarm_resc.py` creates the dynamic Renode monitor script for the
  requested sector, initializes erased journals, resets tag 1, and injects the
  late replay attacker;
- `run_test.sh` invokes the generator, Renode, validation, and report pipeline;
- `nrf52840.repl` is the exact locally patched Renode platform description used
  for every emulated board.

The private provisioning helper and the validator are intentionally not
included. Editing these copies does not change grading: the Run job executes the
immutable copies from its container image.
