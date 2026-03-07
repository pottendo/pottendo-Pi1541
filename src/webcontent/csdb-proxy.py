#!/usr/bin/env python3
"""Minimal reverse proxy for csdb.dk — GET requests only."""

import argparse
import http.client
import http.server
import socketserver
import urllib.parse

PORT = 8001
PREFIX = "/csdb-proxy"
TARGET = "https://csdb.dk"


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if not self.path.startswith(PREFIX):
            self.send_error(404)
            return
        parsed = urllib.parse.urlsplit(TARGET)
        try:
            current = parsed
            stripped = self.path[len(PREFIX):] or "/"
            if not stripped.startswith("/"):
                stripped = "/" + stripped
            print(f"  -> {current.scheme}://{current.netloc}{stripped}")
            path = stripped
            for _ in range(5):
                cls = http.client.HTTPSConnection if current.scheme == "https" else http.client.HTTPConnection
                conn = cls(current.netloc, timeout=10)
                conn.request("GET", path, headers={"Host": current.netloc})
                resp = conn.getresponse()
                body = resp.read()
                conn.close()
                if resp.status in (301, 302, 303, 307, 308):
                    loc = resp.getheader("Location", "")
                    if not loc:
                        break
                    current = urllib.parse.urlsplit(loc)
                    rewritten = args.http and current.scheme == "https" and current.netloc == parsed.netloc
                    if rewritten:
                        current = current._replace(scheme="http")
                        loc = urllib.parse.urlunsplit(current)
                    print(f"     {resp.status} redirect -> {loc}" + (" (http rewrite)" if rewritten else ""))
                    path = current.path + (f"?{current.query}" if current.query else "")
                    continue
                break
        except Exception as e:
            self.send_error(502, f"Bad gateway: {e}")
            return

        print(f"     {resp.status} {resp.reason} ({len(body)} bytes)")
        self.send_response(resp.status)
        for h, v in resp.getheaders():
            if h.lower() in ("transfer-encoding", "content-length"):
                continue
            self.send_header(h, v)
        self.send_header("Content-Length", len(body))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass  # suppress default Apache-style log; we print our own above


parser = argparse.ArgumentParser(description="CSDb reverse proxy")
parser.add_argument("--http", action="store_true", help="Use http instead of https for csdb.dk")
args = parser.parse_args()

if args.http:
    TARGET = TARGET.replace("https://", "http://")

with socketserver.TCPServer(("", PORT), Handler) as httpd:
    print(f"CSDb proxy on http://localhost:{PORT}{PREFIX} -> {TARGET}")
    print("Press Ctrl+C to stop")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped")
