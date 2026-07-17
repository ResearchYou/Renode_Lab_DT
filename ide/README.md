# Browser IDE integration

The existing code-server wrapper remains the participant UI. It mounts three
per-user persistent paths:

- `/home/coder/challenge/firmware` for the seeded GhostTag tag application;
- `/home/coder/challenge/problem` for the rendered task statement;
- `/home/coder/challenge/renode` for the public scripts and platform description
  used to create and launch the Renode scenario.

The Explorer therefore shows exactly `firmware/`, `problem/`, and `renode/`.
UART evidence, JSON, and `report.html` remain on the per-user PVC under
`output/`, but are consumed through the Results panel instead of being a fourth
Explorer root.

The live Kubernetes platform launches the digital-twin image as a queued Job;
the IDE itself does not need the Zephyr SDK. The runtime mounts the same
firmware/output PVCs at `/workspace/firmware` and `/workspace/output`.

Image seed contracts:

- IDE image: `/opt/challenge-seed/{firmware,problem}`;
- digital-twin image: `/workspace/{firmware_seed,problem_seed,renode}`.

The Results panel reads `output/report.html`. It should be refreshed after a
run completes.
