#!/usr/bin/env python3
"""Smoke-test the SwitchBox companion server routes."""

from __future__ import annotations

import json
import shutil
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "companion"))

import server as companion  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


class TestServer:
    def __init__(self, resolver_command: str = "", default_mode: str = "auto"):
        self.state = companion.ServerState(ROOT / ".tmp-companion-test", "ffmpeg", default_mode, False, resolver_command)
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), companion.Handler)
        self.httpd.state = self.state  # type: ignore[attr-defined]
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)

    @property
    def base(self) -> str:
        host, port = self.httpd.server_address
        return f"http://{host}:{port}"

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join(timeout=5)
        shutil.rmtree(self.state.cache_dir, ignore_errors=True)


def get_json(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))


def get_redirect(url: str) -> str:
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(NoRedirect)
    try:
        opener.open(url, timeout=5)
    except urllib.error.HTTPError as exc:
        require(exc.code == 302, f"expected 302, got {exc.code}")
        return exc.headers["Location"]
    raise SystemExit("FAIL: expected redirect")


def main() -> None:
    media_url, headers, header_spec = companion.parse_media_spec("https://origin.example.invalid/live/index.m3u8|User-Agent=SwitchBoxTest&Referer=https%3A%2F%2Fref.example.invalid%2F")
    require(media_url == "https://origin.example.invalid/live/index.m3u8", "media spec url must parse")
    require(headers.get("User-Agent") == "SwitchBoxTest", "media spec User-Agent must parse")
    require(headers.get("Referer") == "https://ref.example.invalid/", "media spec Referer must parse")

    playlist = """#EXTM3U
#EXT-X-KEY:METHOD=AES-128,URI="keys/key.bin"
#EXTINF:6,
seg_00001.ts
#EXTINF:6,
https://cdn.example.invalid/seg_00002.ts
"""
    rewritten = companion.rewrite_m3u8_playlist("https://origin.example.invalid/live/index.m3u8", playlist, header_spec)
    require('/proxy?url=https%3A%2F%2Forigin.example.invalid%2Flive%2Fseg_00001.ts%7CUser-Agent%3DSwitchBoxTest' in rewritten, "relative HLS segment must be proxied with headers")
    require('/proxy?url=https%3A%2F%2Fcdn.example.invalid%2Fseg_00002.ts' in rewritten, "absolute HLS segment must be proxied")
    require('URI="/proxy?url=https%3A%2F%2Forigin.example.invalid%2Flive%2Fkeys%2Fkey.bin%7CUser-Agent%3DSwitchBoxTest' in rewritten, "HLS key URI must be proxied with headers")

    with TestServer() as ts:
        health = get_json(ts.base + "/health")
        require(health.get("ok") is True, "health must be ok")
        require(health.get("defaultMode") == "auto", "default mode must be auto")

        target = "https://example.invalid/video.mp4"
        location = get_redirect(ts.base + "/play?mode=proxy&url=" + urllib.parse.quote(target, safe=""))
        require(location.startswith("/proxy?url="), "proxy mode must redirect to /proxy")
        require(urllib.parse.unquote(location.split("url=", 1)[1]) == target, "proxy redirect must preserve url")

        location = get_redirect(ts.base + "/play?mode=transcode&url=" + urllib.parse.quote(target, safe=""))
        require(location.startswith("/transcode?url="), "transcode mode must redirect to /transcode")
        media_spec = "https://example.invalid/need-headers.m3u8|User-Agent=SwitchBoxTest&Referer=https%3A%2F%2Fref.example.invalid%2F"
        location = get_redirect(ts.base + "/play?mode=proxy&url=" + urllib.parse.quote(media_spec, safe=""))
        require("%7CUser-Agent%3DSwitchBoxTest" in location, "play proxy redirect must preserve TVBox header suffix")

    resolver = f"{sys.executable} {ROOT / 'companion' / 'resolvers' / 'echo_resolver.py'}"
    with TestServer(resolver) as ts:
        health = get_json(ts.base + "/health")
        require(health.get("resolverConfigured") is True, "resolver must be configured")
        target = "https://example.invalid/spider-input"
        location = get_redirect(ts.base + "/play?mode=resolve&url=" + urllib.parse.quote(target, safe=""))
        require(location.startswith("/play?mode=proxy&url="), "resolve mode must redirect to resolved mode")
        require(urllib.parse.unquote(location.split("url=", 1)[1]) == target, "resolver redirect must preserve url")

    with TestServer(default_mode="transcode") as ts:
        target = "https://example.invalid/unknown-container"
        location = get_redirect(ts.base + "/play?mode=auto&url=" + urllib.parse.quote(target, safe=""))
        require(location.startswith("/transcode?url="), "server default-mode transcode must override request auto")

    print("OK: companion server health and play redirects are valid")


if __name__ == "__main__":
    main()
