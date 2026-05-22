#!/usr/bin/env python3
"""Validate SwitchBox demo TVBox/M3U artifacts.

This is a host-side sanity check for environments that do not have devkitPro.
It does not compile the Switch homebrew; it verifies that the example config,
VOD response, and M3U playlist exercise the parser assumptions used by the C++
code.
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def load_json(path: str):
    with (ROOT / path).open("r", encoding="utf-8") as f:
        return json.load(f)


def main() -> None:
    config = load_json("examples/tvbox.demo.json")
    require(isinstance(config.get("sites"), list) and config["sites"], "config.sites must be non-empty")
    require(isinstance(config.get("lives"), list) and config["lives"], "config.lives must be non-empty")
    require(isinstance(config.get("parses"), list), "config.parses must be a list")

    site = config["sites"][0]
    require(site.get("api") == "examples/demo.vod.json", "demo site must point at demo.vod.json")

    vod = load_json("examples/demo.vod.json")
    items = vod.get("list")
    require(isinstance(items, list) and len(items) >= 2, "demo VOD list must contain at least two items")
    for item in items:
        require(item.get("vod_id"), "vod item must have vod_id")
        require(item.get("vod_name"), "vod item must have vod_name")
        require("$" in item.get("vod_play_url", ""), "vod_play_url must contain title$url episodes")

    m3u = (ROOT / "examples/demo.m3u").read_text(encoding="utf-8")
    require("#EXTM3U" in m3u, "M3U must start with #EXTM3U")
    require(m3u.count("#EXTINF") >= 2, "M3U must contain at least two channels")
    require(".m3u8" in m3u, "M3U must contain HLS URLs")

    print("OK: demo TVBox config, VOD response, and M3U playlist are valid")


if __name__ == "__main__":
    main()
