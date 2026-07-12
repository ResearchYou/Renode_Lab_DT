# Browser IDE integration

The existing code-server wrapper remains the participant UI. It mounts three
per-user persistent paths:

- `/home/coder/challenge/firmware` for the seeded GhostTag tag application;
- `/home/coder/challenge/problem` for the rendered task statement;
- `/home/coder/challenge/output` for UART evidence, JSON, and `report.html`.

The live Kubernetes platform launches the digital-twin image as a queued Job;
the IDE itself does not need the Zephyr SDK. The runtime mounts the same
firmware/output PVCs at `/workspace/firmware` and `/workspace/output`.

Image seed contracts:

- IDE image: `/opt/challenge-seed/{firmware,problem}`;
- digital-twin image: `/workspace/{firmware_seed,problem_seed}`.

The Results panel reads `output/report.html`. It should be refreshed after a
run completes.
