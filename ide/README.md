# Browser Challenge IDE

A browser-based IDE for firmware challenges running in a Renode digital twin.

Features:
- multi-file editor with C/C++ LSP via `clangd`
- side panel: task specification + live test results
- mounted challenge files from the host
- Solarized Dark theme

## Start

```bash
./run.sh up --build ide
```

Open `http://localhost:8443` — the wrapper page loads the editor on the left and
the spec/results panel on the right. Use the **Hide Panel** button to toggle it.

Use `run.sh` instead of calling Docker Compose directly when possible; it detects
rootless Docker socket locations and exports `DOCKER_SOCKET_PATH` for the IDE.

## Mounted directories

- `./firmware`      → `/home/coder/challenge/firmware`
- `./ide/problem`   → `/home/coder/challenge/problem`
- `./output`        → `/home/coder/challenge/output`

## Notes

- Place a `problem.md` (or `problem.pdf`) in `./ide/problem/` to populate the spec panel.
- Results panel shows `output/report.html`; click **↻ Refresh** after a test run.
- LSP is provided by `llvm-vs-code-extensions.vscode-clangd`.
