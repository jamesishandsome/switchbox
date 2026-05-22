#!/usr/bin/env python3
"""Example SwitchBox external resolver.

Reads:  {"url":"https://..."}
Writes: {"url":"https://...","mode":"proxy"}

Replace this with a TVBox/Spider/JAR-capable resolver on your PC/NAS.
"""

from __future__ import annotations

import json
import sys


def main() -> int:
    payload = json.loads(sys.stdin.read() or "{}")
    url = payload.get("url", "")
    print(json.dumps({"url": url, "mode": "proxy", "message": "echo resolver"}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
