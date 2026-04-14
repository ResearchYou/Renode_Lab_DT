#!/usr/bin/env python3
"""
Minimal SSE runner for the challenge IDE.

POST /api/run   — trigger a test run and stream output (text/event-stream)
POST /api/stop  — abort the current run
GET  /api/status — {"running": bool}

The runner communicates with the digital-twin daemon via two files in the
shared output volume — no Docker socket access needed:

  .trigger   written by runner  → tells daemon to start a run
  run.log    written by daemon  → runner streams this line-by-line until
                                  a final "EXIT:<code>" line appears
"""

import http.server
import json
import os
import threading
import time

OUTPUT_DIR   = "/home/coder/challenge/output"
TRIGGER_FILE = os.path.join(OUTPUT_DIR, ".trigger")
LOG_FILE     = os.path.join(OUTPUT_DIR, "run.log")
RUN_TIMEOUT  = 300   # seconds before the runner gives up

_lock    = threading.Lock()
_running = [False]
_stop    = [False]


# ── Runner ────────────────────────────────────────────────────────


def run_test(emit):
    with _lock:
        if _running[0]:
            emit("error", "[ERROR] already running")
            emit("done", "1")
            return
        _running[0] = True
        _stop[0] = False

    try:
        _do_run(emit)
    finally:
        with _lock:
            _running[0] = False


def _do_run(emit):
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Truncate log so stale content is never read
    try:
        open(LOG_FILE, "w").close()
    except OSError as exc:
        emit("error", f"[ERROR] cannot clear log: {exc}")
        emit("done", "1")
        return

    # Write trigger — daemon picks this up and starts the test
    try:
        open(TRIGGER_FILE, "w").close()
    except OSError as exc:
        emit("error", f"[ERROR] cannot write trigger: {exc}")
        emit("done", "1")
        return

    emit("info", "$ [digital-twin] run triggered")

    # Stream log file until EXIT:<code> marker or timeout
    deadline = time.monotonic() + RUN_TIMEOUT
    rc = 1

    try:
        with open(LOG_FILE, "r") as fh:
            while time.monotonic() < deadline:
                if _stop[0]:
                    emit("error", "[STOPPED]")
                    return

                line = fh.readline()
                if line:
                    if line.startswith("EXIT:"):
                        rc = int(line[5:].strip())
                        break
                    emit("line", line.rstrip("\n"))
                else:
                    time.sleep(0.1)
            else:
                emit("error", "[ERROR] run timed out")
    except OSError as exc:
        emit("error", f"[ERROR] cannot read log: {exc}")

    emit("done", str(rc))


# ── HTTP handler ──────────────────────────────────────────────────


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/status":
            with _lock:
                running = _running[0]
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

    def _handle_run(self):
        with _lock:
            if _running[0]:
                self._json({"error": "already running"}, status=409)
                return

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("X-Accel-Buffering", "no")
        self._cors()
        self.end_headers()

        def emit(event, data):
            try:
                self.wfile.write(f"event: {event}\ndata: {data}\n\n".encode())
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                pass

        run_test(emit)

    def _handle_stop(self):
        with _lock:
            running = _running[0]
        if running:
            _stop[0] = True
            try:
                os.remove(TRIGGER_FILE)
            except FileNotFoundError:
                pass
            msg = "stopped"
        else:
            msg = "not running"
        self._json({"status": msg})

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
    print("[runner] started — waiting for /api/run requests", flush=True)
    server = http.server.HTTPServer(("127.0.0.1", 3001), Handler)
    server.serve_forever()
