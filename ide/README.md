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

## Notes

- Place a `problem.md` (or `problem.pdf`) in `./ide/problem/` before building the
  IDE image to populate the spec panel for new instances.
- Results panel shows `output/report.html`; click **↻ Refresh** after a test run.
- LSP is provided by `llvm-vs-code-extensions.vscode-clangd`.
