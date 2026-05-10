(function () {
  "use strict";

  var DEFAULT_WIDTH_PX = 420;
  var MIN_WIDTH_PX = 280;
  var MAX_WIDTH_RATIO = 0.7;

  var panelWidth = DEFAULT_WIDTH_PX;
  var isOpen = true;
  var isDragging = false;
  var isRunning = false;
  var streamReader = null;
  var runToken = 0;
  var stopRequested = false;

  function clampWidth(px) {
    var max = Math.floor(window.innerWidth * MAX_WIDTH_RATIO);
    return Math.max(MIN_WIDTH_PX, Math.min(max, px));
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
  }

  var css = [
    ":root { --cp-panel-width: " + DEFAULT_WIDTH_PX + "px; }",

    "#cp-panel-root {",
    "  position: fixed;",
    "  top: 0;",
    "  right: 0;",
    "  height: 100vh;",
    "  width: var(--cp-panel-width);",
    "  z-index: 2147483000;",
    "  display: flex;",
    "  flex-direction: column;",
    "  background: #1e1e1e;",
    "  border-left: 1px solid #3c3c3c;",
    "  box-shadow: -6px 0 24px rgba(0,0,0,.35);",
    "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;",
    "  color: #d4d4d4;",
    "  transition: transform .2s ease, width .15s ease;",
    "}",

    "#cp-panel-root.cp-closed {",
    "  transform: translateX(calc(100% - 14px));",
    "}",

    "#cp-resize-handle {",
    "  position: absolute;",
    "  left: 0;",
    "  top: 0;",
    "  width: 8px;",
    "  height: 100%;",
    "  cursor: ew-resize;",
    "  background: transparent;",
    "}",

    "#cp-resize-handle::after {",
    "  content: '';",
    "  position: absolute;",
    "  left: 2px;",
    "  top: 0;",
    "  width: 2px;",
    "  height: 100%;",
    "  background: #0e639c;",
    "  opacity: .25;",
    "  transition: opacity .15s ease;",
    "}",

    "#cp-resize-handle:hover::after, #cp-resize-handle.cp-dragging::after { opacity: .9; }",

    "#cp-collapse-btn {",
    "  position: fixed;",
    "  top: 50%;",
    "  transform: translateY(-50%);",
    "  right: var(--cp-panel-width);",
    "  z-index: 2147483640;",
    "  width: 14px;",
    "  height: 64px;",
    "  border: 1px solid #3c3c3c;",
    "  border-right: none;",
    "  border-radius: 6px 0 0 6px;",
    "  background: #2d2d2d;",
    "  color: #c5c5c5;",
    "  cursor: pointer;",
    "  padding: 0;",
    "  transition: right .2s ease, background .15s ease;",
    "}",
    "#cp-collapse-btn:hover { background: #3a3d41; }",
    "#cp-panel-root.cp-closed + #cp-collapse-btn { right: 14px; }",

    "#cp-panel-header {",
    "  height: 36px;",
    "  flex: 0 0 36px;",
    "  display: flex;",
    "  align-items: center;",
    "  justify-content: space-between;",
    "  padding: 0 10px 0 14px;",
    "  background: #252526;",
    "  border-bottom: 1px solid #3c3c3c;",
    "  font-size: 12px;",
    "  text-transform: uppercase;",
    "  letter-spacing: .08em;",
    "}",

    "#cp-panel-body {",
    "  flex: 1;",
    "  min-height: 0;",
    "  display: grid;",
    "  grid-template-rows: 1fr 1fr;",
    "}",

    ".cp-block {",
    "  min-height: 0;",
    "  display: flex;",
    "  flex-direction: column;",
    "}",
    ".cp-block + .cp-block { border-top: 2px solid #3c3c3c; }",

    ".cp-block-header {",
    "  flex: 0 0 30px;",
    "  height: 30px;",
    "  display: flex;",
    "  align-items: center;",
    "  justify-content: space-between;",
    "  padding: 0 10px;",
    "  background: #252526;",
    "  border-bottom: 1px solid #3c3c3c;",
    "  font-size: 11px;",
    "  text-transform: uppercase;",
    "  letter-spacing: .06em;",
    "  color: #bdbdbd;",
    "}",

    ".cp-btn {",
    "  appearance: none;",
    "  border: none;",
    "  border-radius: 3px;",
    "  padding: 2px 8px;",
    "  font-size: 11px;",
    "  color: #ddd;",
    "  background: #3a3d41;",
    "  cursor: pointer;",
    "}",
    ".cp-btn:hover { background: #4a4e54; }",
    ".cp-btn[disabled] { opacity: .45; cursor: default; }",
    ".cp-btn.run { background: #0e639c; color: #fff; }",
    ".cp-btn.run:hover { background: #1177bb; }",
    ".cp-btn.stop { background: #6b2020; color: #ffd2d2; }",
    ".cp-btn.stop:hover { background: #862828; }",

    "#cp-terminal {",
    "  flex: 1;",
    "  min-height: 0;",
    "  overflow: auto;",
    "  background: #0d1117;",
    "  color: #e6edf3;",
    "  font-family: 'Cascadia Code','Fira Mono',monospace;",
    "  font-size: 11px;",
    "  line-height: 1.45;",
    "  padding: 8px 10px;",
    "  white-space: pre-wrap;",
    "  word-break: break-word;",
    "}",

    "#cp-terminal .t-info { color: #8b949e; }",
    "#cp-terminal .t-pass { color: #3fb950; }",
    "#cp-terminal .t-fail { color: #f85149; }",
    "#cp-terminal .t-warn { color: #d29922; }",
    "#cp-terminal .t-err { color: #f85149; font-weight: 700; }",
    "#cp-terminal .t-placeholder { color: #666; font-style: italic; }",

    "#cp-results-wrap { flex: 1; min-height: 0; position: relative; }",
    "#cp-results-frame { width: 100%; height: 100%; border: none; display: block; }",
    "#cp-no-report {",
    "  display: none;",
    "  position: absolute;",
    "  inset: 0;",
    "  align-items: center;",
    "  justify-content: center;",
    "  color: #7a7a7a;",
    "  font-family: monospace;",
    "  font-size: 12px;",
    "}",
  ].join("");

  function appendTerminalLine(text, cls) {
    var el = document.getElementById("cp-terminal");
    if (!el) return;
    var span = document.createElement("span");
    if (cls) span.className = cls;
    span.textContent = text + "\n";
    el.appendChild(span);
    el.scrollTop = el.scrollHeight;
  }

  function lineClass(line) {
    if (/^\[PASS\]/.test(line)) return "t-pass";
    if (/^\[FAIL\]/.test(line)) return "t-fail";
    if (/^\[WARN\]/.test(line)) return "t-warn";
    if (/^\[ERROR\]/.test(line)) return "t-err";
    if (/^\[INFO\]/.test(line) || /^\$/.test(line)) return "t-info";
    return "";
  }

  function setRunning(running) {
    isRunning = running;
    var runBtn = document.getElementById("cp-run-btn");
    var stopBtn = document.getElementById("cp-stop-btn");
    if (!runBtn || !stopBtn) return;
    runBtn.disabled = running;
    stopBtn.disabled = !running;
  }

  function reloadResults() {
    var frame = document.getElementById("cp-results-frame");
    var noReport = document.getElementById("cp-no-report");
    if (!frame || !noReport) return;
    noReport.style.display = "none";
    frame.style.display = "block";
    frame.src = "/output/report.html?_=" + Date.now();
  }

  function showNoReport() {
    var frame = document.getElementById("cp-results-frame");
    var noReport = document.getElementById("cp-no-report");
    if (!frame || !noReport) return;
    frame.style.display = "none";
    noReport.style.display = "flex";
  }

  function finishRun(exitCode) {
    setRunning(false);
    if (exitCode === 0) {
      appendTerminalLine("── PASSED (exit 0) ─────────────────", "t-pass");
    } else {
      appendTerminalLine(
        "── FAILED (exit " + exitCode + ") ───────────────",
        "t-fail",
      );
    }
    setTimeout(reloadResults, 500);
  }

  function parseSSEChunkText(raw, onEvent) {
    var blocks = raw.split("\n\n");
    for (var i = 0; i < blocks.length; i++) {
      var block = blocks[i].trim();
      if (!block) continue;
      var eventType = "line";
      var data = "";
      var lines = block.split("\n");
      for (var j = 0; j < lines.length; j++) {
        var l = lines[j];
        if (l.indexOf("event: ") === 0) eventType = l.slice(7).trim();
        if (l.indexOf("data: ") === 0) data = l.slice(6);
      }
      onEvent(eventType, data);
    }
  }

  function startRun() {
    if (isRunning) return;

    var token = ++runToken;
    var completed = false;
    stopRequested = false;

    function complete(exitCode) {
      if (completed || token !== runToken) return;
      completed = true;
      streamReader = null;
      finishRun(exitCode);
    }

    var placeholder = document.querySelector("#cp-terminal .t-placeholder");
    if (placeholder) placeholder.remove();

    appendTerminalLine("── Run started ──────────────────────", "t-info");
    setRunning(true);

    fetch("/api/run", { method: "POST" })
      .then(function (res) {
        if (!res.ok || !res.body)
          throw new Error("Run request failed: " + res.status);
        var reader = res.body.getReader();
        streamReader = reader;
        var decoder = new TextDecoder();
        var buffer = "";

        function readLoop() {
          reader
            .read()
            .then(function (chunk) {
              if (chunk.done) {
                complete(1);
                return;
              }

              buffer += decoder.decode(chunk.value, { stream: true });
              var parts = buffer.split("\n\n");
              buffer = parts.pop() || "";

              parts.forEach(function (block) {
                parseSSEChunkText(block + "\n\n", function (eventType, data) {
                  if (eventType === "done") {
                    complete(parseInt(data, 10) || 0);
                    return;
                  }
                  appendTerminalLine(data, lineClass(data));
                });
              });

              readLoop();
            })
            .catch(function () {
              if (stopRequested && token === runToken) {
                completed = true;
                streamReader = null;
                return;
              }
              streamReader = null;
              complete(1);
            });
        }

        readLoop();
      })
      .catch(function (err) {
        appendTerminalLine("[ERROR] " + err.message, "t-err");
        complete(1);
      });
  }

  function stopRun() {
    stopRequested = true;
    fetch("/api/stop", { method: "POST" }).catch(function () {});
    if (streamReader) {
      try {
        streamReader.cancel();
      } catch (_) {}
      streamReader = null;
    }
    runToken++;
    setRunning(false);
    appendTerminalLine("── Stopped ──────────────────────────", "t-warn");
  }

  function applyWidth() {
    panelWidth = clampWidth(panelWidth);
    document.documentElement.style.setProperty(
      "--cp-panel-width",
      panelWidth + "px",
    );
  }

  function setOpen(open) {
    isOpen = !!open;
    var root = document.getElementById("cp-panel-root");
    var btn = document.getElementById("cp-collapse-btn");
    if (!root || !btn) return;

    if (isOpen) {
      root.classList.remove("cp-closed");
      btn.innerHTML = "&#8250;";
      btn.title = "Collapse side panel";
    } else {
      root.classList.add("cp-closed");
      btn.innerHTML = "&#8249;";
      btn.title = "Expand side panel";
    }
  }

  function startDrag(e) {
    if (!isOpen) return;
    isDragging = true;
    var handle = document.getElementById("cp-resize-handle");
    if (handle) handle.classList.add("cp-dragging");
    document.body.style.userSelect = "none";
    document.body.style.cursor = "ew-resize";
    e.preventDefault();
  }

  function onDrag(e) {
    if (!isDragging) return;
    panelWidth = clampWidth(window.innerWidth - e.clientX);
    applyWidth();
  }

  function stopDrag() {
    if (!isDragging) return;
    isDragging = false;
    var handle = document.getElementById("cp-resize-handle");
    if (handle) handle.classList.remove("cp-dragging");
    document.body.style.userSelect = "";
    document.body.style.cursor = "";
  }

  function renderPanel() {
    if (document.getElementById("cp-panel-root")) return;

    var style = document.createElement("style");
    style.id = "cp-panel-style";
    style.textContent = css;
    document.head.appendChild(style);

    var root = document.createElement("aside");
    root.id = "cp-panel-root";
    root.setAttribute("aria-label", "Challenge side panel");
    root.innerHTML =
      '<div id="cp-resize-handle" title="Resize panel"></div>' +
      '<div id="cp-panel-header"><span>Challenge Panel</span><span style="font-size:11px;color:#8b8b8b">Overlay</span></div>' +
      '<div id="cp-panel-body">' +
      '<section class="cp-block">' +
      '<div class="cp-block-header">' +
      "<span>Run</span>" +
      "<div>" +
      '<button id="cp-run-btn" class="cp-btn run" title="Run tests">▶ Run</button> ' +
      '<button id="cp-stop-btn" class="cp-btn stop" title="Stop run" disabled>■ Stop</button>' +
      "</div>" +
      "</div>" +
      '<div id="cp-terminal"><span class="t-placeholder">Click ▶ Run to execute the test suite.</span></div>' +
      "</section>" +
      '<section class="cp-block">' +
      '<div class="cp-block-header">' +
      "<span>Test Results</span>" +
      '<button id="cp-refresh-btn" class="cp-btn" title="Refresh report">↻ Refresh</button>' +
      "</div>" +
      '<div id="cp-results-wrap">' +
      '<iframe id="cp-results-frame" src="/output/report.html"></iframe>' +
      '<div id="cp-no-report">No report yet.</div>' +
      "</div>" +
      "</section>" +
      "</div>";

    var collapseBtn = document.createElement("button");
    collapseBtn.id = "cp-collapse-btn";
    collapseBtn.type = "button";
    collapseBtn.innerHTML = "&#8250;";
    collapseBtn.title = "Collapse side panel";

    document.body.appendChild(root);
    document.body.appendChild(collapseBtn);

    applyWidth();
    setOpen(true);

    document
      .getElementById("cp-resize-handle")
      .addEventListener("mousedown", startDrag);
    document.addEventListener("mousemove", onDrag);
    document.addEventListener("mouseup", stopDrag);

    collapseBtn.addEventListener("click", function () {
      setOpen(!isOpen);
    });

    document.getElementById("cp-run-btn").addEventListener("click", startRun);
    document.getElementById("cp-stop-btn").addEventListener("click", stopRun);
    document
      .getElementById("cp-refresh-btn")
      .addEventListener("click", reloadResults);

    var frame = document.getElementById("cp-results-frame");
    frame.addEventListener("error", showNoReport);
    frame.addEventListener("load", function () {
      try {
        var title =
          (frame.contentDocument && frame.contentDocument.title) || "";
        if (!title || /404|error/i.test(title)) showNoReport();
      } catch (_) {}
    });
  }

  function boot() {
    if (!document.body || !document.head) {
      setTimeout(boot, 50);
      return;
    }

    try {
      renderPanel();
    } catch (e) {
      var msg =
        "[panel] failed to initialize: " +
        (e && e.message ? e.message : String(e));
      try {
        console.error(msg);
      } catch (_) {}
      var pre = document.createElement("pre");
      pre.style.cssText =
        "position:fixed;bottom:8px;right:8px;z-index:2147483647;background:#2b0000;color:#ffb4b4;padding:8px;max-width:40vw;white-space:pre-wrap;font:12px monospace;";
      pre.textContent = escapeHtml(msg);
      document.body.appendChild(pre);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
