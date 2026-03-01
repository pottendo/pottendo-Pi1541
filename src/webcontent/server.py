#!/usr/bin/env python3
"""
Simple Python HTTP server for 1541UI

Serves static files from the web/ directory and provides two reverse proxies:
  /proxy/*  -> Pi1541 device (default http://192.168.178.91)
  /csdb/*   -> CSDB (https://csdb.dk)

Proxied responses are passed through unchanged (including gzip encoding);
only transfer-encoding is stripped since the body is already de-chunked.
Server-side redirect following keeps the browser on the proxy origin.
CORS headers are injected on all proxied responses.

Usage:  python server.py        # serves on http://localhost:8000
"""

import http.client
import http.server
import os
import socketserver
import urllib.parse
from pathlib import Path

# Configuration
PORT = 8000
UI_DIR = "web"
PROXY_PREFIX = "/proxy"
PROXY_TARGET = "http://192.168.1.31"
CSDB_PREFIX = "/csdb"
CSDB_TARGET = "https://csdb.dk"


class SimpleHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=UI_DIR, **kwargs)

    def _get_proxy_target(self):
        if self.path.startswith(CSDB_PREFIX):
            return CSDB_PREFIX, CSDB_TARGET
        if self.path.startswith(PROXY_PREFIX):
            return PROXY_PREFIX, PROXY_TARGET
        return None, None

    def _handle_method(self, default_handler=None):
        prefix, _ = self._get_proxy_target()
        if prefix:
            self.forward_request()
        elif default_handler:
            default_handler()
        else:
            self.send_error(405)

    def do_OPTIONS(self):
        prefix, _ = self._get_proxy_target()
        if prefix:
            self.send_response(200)
            self.send_cors_headers()
            self.end_headers()
        else:
            super().do_OPTIONS()

    def do_GET(self):
        self._handle_method(super().do_GET)

    def do_POST(self):
        self._handle_method()

    def do_PUT(self):
        self._handle_method()

    def do_DELETE(self):
        self._handle_method()

    def do_PATCH(self):
        self._handle_method()

    def send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header(
            "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, PATCH"
        )
        req_headers = self.headers.get("Access-Control-Request-Headers")
        if req_headers:
            self.send_header("Access-Control-Allow-Headers", req_headers)
        else:
            self.send_header("Access-Control-Allow-Headers", "*")

    def forward_request(self):
        # Determine which proxy target to use
        prefix, target = self._get_proxy_target()
        # Build target URL path
        parsed_target = urllib.parse.urlsplit(target)
        target_path = self.path[len(prefix):] or "/"
        if not target_path.startswith("/"):
            target_path = "/" + target_path
        # Prepare connection
        conn_class = (
            http.client.HTTPSConnection
            if parsed_target.scheme == "https"
            else http.client.HTTPConnection
        )
        # Read body
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length > 0 else None
        # Prepare headers
        forward_headers = {}
        for k, v in self.headers.items():
            # Skip Host header, will be set to target
            if k.lower() == "host":
                continue
            forward_headers[k] = v
        forward_headers["Host"] = parsed_target.netloc
        try:
            # Follow redirects server-side so the browser always stays on the proxy origin
            current_path = target_path
            max_redirects = 5
            for _ in range(max_redirects):
                conn = conn_class(parsed_target.netloc, timeout=10)
                conn.request(self.command, current_path, body=body, headers=forward_headers)
                resp = conn.getresponse()
                resp_body = resp.read()
                conn.close()
                if resp.status in (301, 302, 303, 307, 308):
                    location = resp.getheader("Location", "")
                    if not location:
                        break
                    parsed_loc = urllib.parse.urlsplit(location)
                    # Follow cross-origin redirects by updating the connection target
                    if parsed_loc.netloc and parsed_loc.netloc != parsed_target.netloc:
                        parsed_target = parsed_loc
                        conn_class = (
                            http.client.HTTPSConnection
                            if parsed_loc.scheme == "https"
                            else http.client.HTTPConnection
                        )
                        forward_headers["Host"] = parsed_loc.netloc
                    new_path = parsed_loc.path or "/"
                    if parsed_loc.query:
                        new_path += "?" + parsed_loc.query
                    current_path = new_path
                    # 303 always converts to GET; reuse GET for 301/302 as browsers do
                    if resp.status in (301, 302, 303):
                        self.command = "GET"
                        body = None
                    continue
                break
            self.send_response(resp.status, resp.reason)
            self.log_message("  -> %s%s", target, current_path)
            # Forward headers; skip transfer-encoding (already de-chunked)
            # and content-length (we set our own based on actual body size)
            for h, val in resp.getheaders():
                if h.lower() in ("transfer-encoding", "content-length"):
                    continue
                self.send_header(h, val)
            self.send_header("Content-Length", len(resp_body) if resp_body else 0)
            self.send_cors_headers()
            self.end_headers()
            if resp_body:
                self.wfile.write(resp_body)
        except Exception as e:
            self.send_error(502, f"Bad gateway: {e}")


def run_server():
    """Start the HTTP server"""
    # Change to the directory containing the ui folder
    os.chdir(Path(__file__).parent)

    print(f"1541UI Server - Serving from './{UI_DIR}' directory")
    print(f"Available at: http://localhost:{PORT}")
    print("Press Ctrl+C to stop the server")

    with socketserver.TCPServer(("", PORT), SimpleHTTPRequestHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped")


if __name__ == "__main__":
    run_server()
