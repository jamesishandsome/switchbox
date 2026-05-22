#!/usr/bin/env python3
"""End-to-end FFmpeg transcode smoke test for SwitchBox Companion."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.parse
import urllib.request
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "companion"))

import server as companion  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


class ManagedServer:
    def __init__(self, httpd):
        self.httpd = httpd
        self.thread = threading.Thread(target=httpd.serve_forever, daemon=True)

    @property
    def base(self) -> str:
        host, port = self.httpd.server_address
        return f"http://{host}:{port}"

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        if hasattr(self.httpd, "state"):
            for proc in self.httpd.state.processes.values():  # type: ignore[attr-defined]
                if proc.poll() is None:
                    proc.terminate()
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join(timeout=5)


def make_static_server(directory: Path) -> ManagedServer:
    handler = partial(SimpleHTTPRequestHandler, directory=str(directory))
    return ManagedServer(ThreadingHTTPServer(("127.0.0.1", 0), handler))


def make_companion_server(cache_dir: Path) -> ManagedServer:
    state = companion.ServerState(cache_dir, "ffmpeg", "transcode", True)
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), companion.Handler)
    httpd.state = state  # type: ignore[attr-defined]
    return ManagedServer(httpd)


def no_redirect_location(url: str) -> str:
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(NoRedirect)
    try:
        opener.open(url, timeout=20)
    except urllib.error.HTTPError as exc:
        require(exc.code == 302, f"expected 302, got {exc.code}")
        return exc.headers["Location"]
    raise SystemExit("FAIL: expected redirect")


def main() -> None:
    require(shutil.which("ffmpeg") is not None, "ffmpeg must be on PATH")
    with tempfile.TemporaryDirectory(prefix="switchbox-transcode-") as td:
        root = Path(td)
        media = root / "sample.mp4"
        cache = root / "cache"
        subprocess.run([
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=160x90:rate=5",
            "-f", "lavfi", "-i", "sine=frequency=1000:sample_rate=44100",
            "-t", "1",
            "-c:v", "libx264", "-pix_fmt", "yuv420p",
            "-c:a", "aac",
            str(media),
        ], check=True)
        require(media.exists() and media.stat().st_size > 0, "sample mp4 must be generated")

        with make_static_server(root) as static, make_companion_server(cache) as comp:
            media_url = static.base + "/sample.mp4"
            location = no_redirect_location(comp.base + "/transcode?url=" + urllib.parse.quote(media_url, safe=""))
            require(location.startswith("/hls/"), "transcode must redirect to hls playlist")
            with urllib.request.urlopen(comp.base + location, timeout=20) as resp:
                playlist = resp.read().decode("utf-8", errors="replace")
            require("#EXTM3U" in playlist, "HLS playlist must contain #EXTM3U")
            require(".ts" in playlist, "HLS playlist must reference segments")

    print("OK: ffmpeg transcode smoke test produced HLS playlist")


if __name__ == "__main__":
    main()
