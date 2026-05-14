# Browser Challenge IDE

A browser-based IDE for firmware challenges running in a Renode digital twin.

Features:
- multi-file editor with C/C++ LSP via `clangd`
- side panel: task specification + live test results
- per-instance persistent challenge storage
- Solarized Dark theme

## Start

```bash
./run.sh up --build ide
```

Open `http://localhost:8443` — the wrapper page loads the editor on the left and
the spec/results panel on the right. Use the **Hide Panel** button to toggle it.

Use `run.sh` instead of calling Docker Compose directly when possible; it detects
rootless Docker socket locations and exports `DOCKER_SOCKET_PATH` for the IDE.

## Multiple instances

Run each user/session with a distinct Compose project name and host port:

```bash
COMPOSE_PROJECT_NAME=user1 IDE_PORT=8443 ./run.sh up --build -d ide
COMPOSE_PROJECT_NAME=user2 IDE_PORT=8444 ./run.sh up --build -d ide
```

Each Compose project gets separate persistent Docker volumes:

- `challenge-firmware` -> `/home/coder/challenge/firmware`
- `challenge-problem` -> `/home/coder/challenge/problem`
- `challenge-output` -> `/home/coder/challenge/output`

The volumes are seeded from the image on first start. They remain after
containers are removed; use `./run.sh down -v` only when you want to delete that
instance's stored code and output.

## Kubernetes storage

Kubernetes owns persistence. Mount per-user PersistentVolumeClaims at the same
paths used by Compose:

- `/home/coder/challenge/firmware` in the IDE and `/workspace/firmware` in the
  digital twin
- `/home/coder/challenge/output` in the IDE and `/workspace/output` in the
  digital twin
- `/home/coder/challenge/problem` in the IDE

The IDE image stores default challenge files under `/opt/challenge-seed` and
copies them into empty firmware/problem mounts on startup. This matters in
Kubernetes because PVC mounts hide image files and are not automatically
initialized from the image.

Use a separate PVC set per user/session if users should be isolated. If platform
admins need direct access to saved code, back those PVCs with a storage class
that supports the desired access workflow, for example snapshots, RWX/NFS, or
mounting the PVC into an admin/debug pod.

## Notes

- Place a `problem.md` (or `problem.pdf`) in `./ide/problem/` before building the
  IDE image to populate the spec panel for new instances.
- Results panel shows `output/report.html`; click **↻ Refresh** after a test run.
- LSP is provided by `llvm-vs-code-extensions.vscode-clangd`.
