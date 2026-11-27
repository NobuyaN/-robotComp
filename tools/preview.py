#!/usr/bin/env python3
"""Serve the embedded controller with a simulated connection; no ESP32 needed."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOCK_CONNECTION = """<script>
// Preview only: acknowledge commands locally. Never open a hardware connection.
window.WebSocket = class PreviewSocket {
  static OPEN = 1;
  constructor() {
    this.readyState = 1;
    this.bufferedAmount = 0;
    setTimeout(() => {
      if (this.readyState === 1) this.onmessage?.({data: 'ready'});
    }, 0);
  }
  send() {
    setTimeout(() => {
      if (this.readyState === 1) this.onmessage?.({data: 'ok'});
    }, 0);
  }
  close() {
    if (this.readyState === 3) return;
    this.readyState = 3;
    this.onclose?.();
  }
};
</script>"""


def preview_html():
    source = (ROOT / "include" / "web_page.h").read_text(encoding="utf-8")
    html = source.split('R"HTML(', 1)[1].split(')HTML";', 1)[0]
    html = html.replace("<title>RobotCar controller</title>",
                        "<title>RobotCar — local preview</title>")
    html = html.replace("<h1>RobotCar</h1>",
                        "<h1>RobotCar</h1><p><strong>PREVIEW — no hardware connected</strong></p>")
    html = html.replace("Connected — hold joystick to drive.",
                        "Preview ready — drag the joystick to try it.")
    return html.replace("<script>", MOCK_CONNECTION + "<script>", 1).encode("utf-8")


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path not in ("/", "/index.html"):
            self.send_error(404)
            return
        content = preview_html()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"RobotCar preview: http://127.0.0.1:{args.port} (Ctrl+C to stop)", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
