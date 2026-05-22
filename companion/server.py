#!/usr/bin/env python3
"""SwitchBox Companion Server.

Run this on a PC/NAS in the same LAN as the Switch to improve playback
compatibility. The Switch opens /play?url=...; this server can proxy or
transcode the upstream video into a Switch-friendlier URL.

No content sources are bundled. Use only media URLs you are allowed to access.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Dict, Tuple

USER_AGENT = "SwitchBoxCompanion/0.1"
HLS_START_TIMEOUT_SECONDS = 12


def sha1_text(value: str) -> str:
    return hashlib.sha1(value.encode("utf-8")).hexdigest()


def safe_url(value: str) -> bool:
    parsed = urllib.parse.urlparse(value)
    return parsed.scheme in {"http", "https"} and bool(parsed.netloc)


def ffmpeg_exists(ffmpeg: str) -> bool:
    return shutil.which(ffmpeg) is not None or Path(ffmpeg).exists()


def parse_media_spec(value: str) -> Tuple[str, Dict[str, str], str]:
    """Parse TVBox-style `url|Header=Value&Header2=Value2` media specs."""
    if "|" not in value:
        return value, {}, ""
    url, header_spec = value.split("|", 1)
    headers: Dict[str, str] = {}
    for key, vals in urllib.parse.parse_qs(header_spec, keep_blank_values=True).items():
        if not vals:
            continue
        normalized = key.strip()
        if not normalized:
            continue
        if normalized.lower() in {"user-agent", "referer", "referrer", "origin", "cookie", "authorization"}:
            if normalized.lower() == "referrer":
                normalized = "Referer"
            headers[normalized] = vals[-1]
    return url, headers, header_spec


def proxied_path(url: str, header_spec: str = "") -> str:
    spec = url + ("|" + header_spec if header_spec else "")
    return "/proxy?url=" + urllib.parse.quote(spec, safe="")


def rewrite_m3u8_playlist(base_url: str, text: str, header_spec: str = "") -> str:
    """Rewrite HLS child playlists, segments, and key URIs through /proxy."""
    out = []
    key_uri = re.compile(r'URI="([^"]+)"')
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            out.append(raw_line)
            continue
        if line.startswith("#EXT-X-KEY") and "URI=" in line:
            def repl(match: re.Match[str]) -> str:
                absolute = urllib.parse.urljoin(base_url, match.group(1))
                return 'URI="' + proxied_path(absolute, header_spec) + '"'
            out.append(key_uri.sub(repl, raw_line))
            continue
        if line.startswith("#"):
            out.append(raw_line)
            continue
        absolute = urllib.parse.urljoin(base_url, line)
        out.append(proxied_path(absolute, header_spec))
    return "\n".join(out) + "\n"


class ServerState:
    def __init__(self, cache_dir: Path, ffmpeg: str, default_mode: str, allow_transcode: bool, resolver_command: str = ""):
        self.cache_dir = cache_dir
        self.ffmpeg = ffmpeg
        self.default_mode = default_mode
        self.allow_transcode = allow_transcode
        self.resolver_command = resolver_command
        self.processes: Dict[str, subprocess.Popen] = {}
        self.cache_dir.mkdir(parents=True, exist_ok=True)

    def hls_dir(self, media_spec: str) -> Path:
        return self.cache_dir / sha1_text(media_spec)

    def ensure_transcode(self, media_spec: str) -> Tuple[bool, str]:
        if not self.allow_transcode:
            return False, "transcode disabled; start with --allow-transcode"
        if not ffmpeg_exists(self.ffmpeg):
            return False, f"ffmpeg not found: {self.ffmpeg}"

        media_url, extra_headers, _header_spec = parse_media_spec(media_spec)
        if not safe_url(media_url):
            return False, "unsafe transcode url"

        key = sha1_text(media_spec)
        out_dir = self.hls_dir(media_spec)
        playlist = out_dir / "index.m3u8"
        out_dir.mkdir(parents=True, exist_ok=True)

        proc = self.processes.get(key)
        if proc and proc.poll() is None and playlist.exists():
            return True, "already running"

        if playlist.exists():
            return True, "cached"

        cmd = [
            self.ffmpeg,
            "-hide_banner",
            "-loglevel", "warning",
            "-y",
            "-user_agent", USER_AGENT,
        ]
        if extra_headers:
            header_lines = "".join(f"{key}: {value}\r\n" for key, value in extra_headers.items())
            cmd.extend(["-headers", header_lines])
        cmd.extend([
            "-i", media_url,
            "-map", "0:v:0?",
            "-map", "0:a:0?",
            "-c:v", "libx264",
            "-preset", "veryfast",
            "-profile:v", "main",
            "-level", "4.0",
            "-pix_fmt", "yuv420p",
            "-vf", "scale='min(1280,iw)':-2",
            "-c:a", "aac",
            "-b:a", "128k",
            "-ac", "2",
            "-f", "hls",
            "-hls_time", "6",
            "-hls_list_size", "0",
            "-hls_flags", "independent_segments",
            "-hls_segment_filename", str(out_dir / "seg_%05d.ts"),
            str(playlist),
        ])
        log_file = open(out_dir / "ffmpeg.log", "ab")
        proc = subprocess.Popen(cmd, stdout=log_file, stderr=log_file)
        self.processes[key] = proc

        deadline = time.time() + HLS_START_TIMEOUT_SECONDS
        while time.time() < deadline:
            if playlist.exists():
                return True, "started"
            if proc.poll() is not None:
                return False, f"ffmpeg exited with code {proc.returncode}; see {out_dir / 'ffmpeg.log'}"
            time.sleep(0.25)
        return True, "starting"

    def resolve_url(self, url: str) -> Tuple[bool, str, str, str]:
        """Return ok, resolved_url, mode, message."""
        if not self.resolver_command:
            return False, url, self.default_mode, "resolver command is not configured"

        args = shlex.split(self.resolver_command, posix=os.name != "nt")
        payload = json.dumps({"url": url}, ensure_ascii=False).encode("utf-8")
        try:
            proc = subprocess.run(args, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, check=False)
        except Exception as exc:  # noqa: BLE001 - surface local resolver failures to LAN UI.
            return False, url, self.default_mode, f"resolver failed: {exc}"
        if proc.returncode != 0:
            stderr = proc.stderr.decode("utf-8", errors="replace").strip()
            return False, url, self.default_mode, f"resolver exited {proc.returncode}: {stderr}"
        try:
            data = json.loads(proc.stdout.decode("utf-8"))
        except json.JSONDecodeError as exc:
            return False, url, self.default_mode, f"resolver returned invalid JSON: {exc}"
        resolved = data.get("url") or data.get("playUrl") or url
        mode = data.get("mode") or self.default_mode
        message = data.get("message") or "resolved"
        if not safe_url(resolved):
            return False, url, self.default_mode, "resolver returned unsafe url"
        return True, resolved, mode, message


class Handler(BaseHTTPRequestHandler):
    server_version = "SwitchBoxCompanion/0.1"

    @property
    def state(self) -> ServerState:
        return self.server.state  # type: ignore[attr-defined]

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("[%s] %s\n" % (self.log_date_time_string(), fmt % args))

    def query(self) -> Dict[str, str]:
        parsed = urllib.parse.urlparse(self.path)
        return {k: v[-1] if v else "" for k, v in urllib.parse.parse_qs(parsed.query).items()}

    def send_json(self, data: dict, status: int = 200) -> None:
        body = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_error_json(self, message: str, status: int = 400) -> None:
        self.send_json({"ok": False, "error": message}, status)

    def redirect(self, location: str) -> None:
        self.send_response(HTTPStatus.FOUND)
        self.send_header("Location", location)
        self.end_headers()

    def do_GET(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        if path == "/health":
            self.send_json({
                "ok": True,
                "defaultMode": self.state.default_mode,
                "allowTranscode": self.state.allow_transcode,
                "ffmpegFound": ffmpeg_exists(self.state.ffmpeg),
                "resolverConfigured": bool(self.state.resolver_command),
            })
            return
        if path == "/play":
            self.handle_play()
            return
        if path == "/proxy":
            self.handle_proxy()
            return
        if path == "/transcode":
            self.handle_transcode()
            return
        if path == "/fetch":
            self.handle_fetch()
            return
        if path.startswith("/hls/"):
            self.handle_hls(path)
            return
        self.send_error_json("not found", 404)

    def handle_play(self) -> None:
        q = self.query()
        media_spec = q.get("url", "")
        media_url, _extra_headers, _header_spec = parse_media_spec(media_spec)
        mode = q.get("mode", "auto") or self.state.default_mode
        if mode == "auto" and self.state.default_mode != "auto":
            mode = self.state.default_mode
        if not safe_url(media_url):
            self.send_error_json("missing or unsafe url")
            return
        if mode == "direct":
            self.redirect(media_url)
            return
        if mode == "transcode":
            self.redirect("/transcode?url=" + urllib.parse.quote(media_spec, safe=""))
            return
        if mode == "resolve":
            ok, resolved, resolved_mode, message = self.state.resolve_url(media_spec)
            if not ok:
                self.send_error_json(message, 502)
                return
            target_mode = resolved_mode if resolved_mode != "resolve" else self.state.default_mode
            self.redirect("/play?mode=" + urllib.parse.quote(target_mode, safe="") + "&url=" + urllib.parse.quote(resolved, safe=""))
            return
        if mode == "proxy":
            self.redirect("/proxy?url=" + urllib.parse.quote(media_spec, safe=""))
            return
        # auto: first try a proxy URL. Users can set playbackMode=transcode for stubborn sources.
        if self.state.resolver_command:
            ok, resolved, resolved_mode, _message = self.state.resolve_url(media_spec)
            if ok:
                target_mode = resolved_mode if resolved_mode != "resolve" else self.state.default_mode
                self.redirect("/play?mode=" + urllib.parse.quote(target_mode, safe="") + "&url=" + urllib.parse.quote(resolved, safe=""))
                return
        self.redirect("/proxy?url=" + urllib.parse.quote(media_spec, safe=""))

    def handle_fetch(self) -> None:
        q = self.query()
        url = q.get("url", "")
        if not safe_url(url):
            self.send_error_json("missing or unsafe url")
            return
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=20) as resp:
                body = resp.read()
                content_type = resp.headers.get("Content-Type", "application/octet-stream")
        except urllib.error.URLError as exc:
            self.send_error_json(str(exc), 502)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def handle_proxy(self) -> None:
        q = self.query()
        media_spec = q.get("url", "")
        media_url, extra_headers, header_spec = parse_media_spec(media_spec)
        if not safe_url(media_url):
            self.send_error_json("missing or unsafe url")
            return
        headers = {"User-Agent": USER_AGENT}
        headers.update(extra_headers)
        if "Range" in self.headers:
            headers["Range"] = self.headers["Range"]
        try:
            req = urllib.request.Request(media_url, headers=headers)
            with urllib.request.urlopen(req, timeout=30) as resp:
                status = resp.status
                content_type = resp.headers.get("Content-Type", "")
                is_hls = "mpegurl" in content_type.lower() or urllib.parse.urlparse(media_url).path.endswith(".m3u8")
                if is_hls:
                    raw = resp.read().decode("utf-8", errors="replace")
                    body = rewrite_m3u8_playlist(media_url, raw, header_spec).encode("utf-8")
                    self.send_response(status)
                    self.send_header("Content-Type", "application/vnd.apple.mpegurl; charset=utf-8")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    return
                self.send_response(status)
                for name in ["Content-Type", "Content-Length", "Accept-Ranges", "Content-Range"]:
                    value = resp.headers.get(name)
                    if value:
                        self.send_header(name, value)
                if not resp.headers.get("Content-Type"):
                    self.send_header("Content-Type", "application/octet-stream")
                self.end_headers()
                shutil.copyfileobj(resp, self.wfile)
        except urllib.error.HTTPError as exc:
            self.send_error_json(f"upstream http {exc.code}", 502)
        except urllib.error.URLError as exc:
            self.send_error_json(str(exc), 502)

    def handle_transcode(self) -> None:
        q = self.query()
        media_spec = q.get("url", "")
        media_url, _extra_headers, _header_spec = parse_media_spec(media_spec)
        if not safe_url(media_url):
            self.send_error_json("missing or unsafe url")
            return
        ok, message = self.state.ensure_transcode(media_spec)
        if not ok:
            self.send_error_json(message, 500)
            return
        key = sha1_text(media_spec)
        self.redirect(f"/hls/{key}/index.m3u8")

    def handle_hls(self, path: str) -> None:
        parts = path.strip("/").split("/")
        if len(parts) != 3 or parts[0] != "hls":
            self.send_error_json("bad hls path", 404)
            return
        key, filename = parts[1], parts[2]
        if not all(c in "0123456789abcdef" for c in key) or len(key) != 40:
            self.send_error_json("bad hls key", 400)
            return
        if "/" in filename or ".." in filename:
            self.send_error_json("bad hls filename", 400)
            return
        file_path = self.state.cache_dir / key / filename
        if not file_path.exists():
            self.send_error_json("hls file not ready", 404)
            return
        ctype = "application/vnd.apple.mpegurl" if filename.endswith(".m3u8") else mimetypes.guess_type(filename)[0] or "application/octet-stream"
        data = file_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SwitchBox LAN companion server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8099)
    parser.add_argument("--cache-dir", default=".switchbox-cache")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--default-mode", choices=["auto", "proxy", "transcode", "direct", "resolve"], default="auto")
    parser.add_argument("--allow-transcode", action="store_true", help="Enable FFmpeg HLS transcoding endpoint")
    parser.add_argument("--resolver-command", default="", help="External resolver command. Receives JSON on stdin and prints JSON with url/mode.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    state = ServerState(Path(args.cache_dir), args.ffmpeg, args.default_mode, args.allow_transcode, args.resolver_command)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.state = state  # type: ignore[attr-defined]
    print(f"SwitchBox Companion listening on http://{args.host}:{args.port}")
    print(f"Health: http://127.0.0.1:{args.port}/health")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        for proc in state.processes.values():
            if proc.poll() is None:
                proc.terminate()
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
