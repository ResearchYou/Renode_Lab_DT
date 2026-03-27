#!/usr/bin/env python3
"""
Minimal SSE runner for the challenge IDE.

POST /api/run   — stream test run output (text/event-stream)
POST /api/stop  — kill the running process
GET  /api/status — {"running": bool}
"""

import http.server
import json
import os
import subprocess
import threading

COMPOSE_FILE = "/home/coder/docker-compose.yml"
PROJECT_DIR = os.environ.get("HOST_PROJECT_DIR", "")
RUNNER_SERVICE = os.environ.get("RUNNER_SERVICE", "digital-twin")

_lock = threading.Lock()
_current = None  # active Popen


# ── Runner ────────────────────────────────────────────────────────


def run_test(emit):
    global _current

    if not PROJECT_DIR:
        emit("error", "[ERROR] HOST_PROJECT_DIR is not set — cannot locate project")
        emit("done", "1")
        return

    try:
        os.makedirs(PROJECT_DIR, exist_ok=True)
        os.makedirs(os.path.join(PROJECT_DIR, "docker"), exist_ok=True)
        with open(os.path.join(PROJECT_DIR, "docker", "Dockerfile"), "a"):
            pass
        with open(os.path.join(PROJECT_DIR, "docker", "Dockerfile.ide"), "a"):
            pass
    except Exception as exc:
        emit("error", f"[ERROR] failed to create HOST_PROJECT_DIR: {exc}")
        emit("done", "1")
        return

    cmd = [
        "docker-compose",
        "-f",
        COMPOSE_FILE,
        "--project-directory",
        PROJECT_DIR,
        "run",
        "--rm",
        RUNNER_SERVICE,
    ]
    emit("info", "$ " + " ".join(cmd))

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except Exception as exc:
        emit("error", f"[ERROR] failed to start: {exc}")
        emit("done", "1")
        return

    with _lock:
        _current = proc

    for line in proc.stdout:
        emit("line", line.rstrip("\n"))

    proc.wait()

    with _lock:
        _current = None

    emit("done", str(proc.returncode))


# ── HTTP handler ──────────────────────────────────────────────────


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    # ── CORS pre-flight ──
    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/status":
            with _lock:
                running = _current is not None
            self._json({"running": running})
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if self.path == "/api/run":
            self._handle_run()
        elif self.path == "/api/stop":
            self._handle_stop()
        else:
            self.send_response(404)
            self.end_headers()

    # ── /api/run — SSE stream ──
    def _handle_run(self):
        with _lock:
            if _current is not None:
                self._json({"error": "already running"}, status=409)
                return

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("X-Accel-Buffering", "no")  # tell nginx not to buffer SSE
        self._cors()
        self.end_headers()

        def emit(event, data):
            try:
                self.wfile.write(f"event: {event}\ndata: {data}\n\n".encode())
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                pass

        run_test(emit)

    # ── /api/stop ──
    def _handle_stop(self):
        global _current
        with _lock:
            proc = _current
        if proc:
            proc.terminate()
            msg = "stopped"
        else:
            msg = "not running"
        self._json({"status": msg})

    # ── helpers ──
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")

    def _json(self, obj, status=200):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self._cors()
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    print(
        f"[runner] service={RUNNER_SERVICE!r}  project={PROJECT_DIR!r}",
        flush=True,
    )
    server = http.server.HTTPServer(("127.0.0.1", 3001), Handler)
    server.serve_forever()
