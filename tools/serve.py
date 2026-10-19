#!/usr/bin/env python3
"""Dev/preview server with no-cache headers.

Serves the repository root so the preview iframe always revalidates and never
runs stale JavaScript after a deploy:

    python3 tools/serve.py [port]        # default port 8000
"""
import http.server
import os
import socketserver
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, must-revalidate")
        self.send_header("Expires", "0")
        super().end_headers()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    with Server(("0.0.0.0", port), Handler) as httpd:
        print(f"serving {ROOT} on http://0.0.0.0:{port} (no-cache)")
        httpd.serve_forever()
