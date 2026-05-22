# Build and Test

## Host-side validators

Run these anywhere with Python 3:

```sh
python -m py_compile companion/server.py companion/resolvers/echo_resolver.py tools/validate_examples.py tools/validate_companion.py
python tools/validate_examples.py
python tools/validate_companion.py
python tools/validate_ffmpeg_transcode.py  # optional, requires ffmpeg
```

They validate:

- demo TVBox JSON and demo VOD response shape;
- demo M3U playlist;
- companion `/health`;
- `/play` redirects for proxy/transcode/resolve;
- server `default-mode` override;
- HLS playlist rewrite;
- TVBox `url|User-Agent=...&Referer=...` header suffix parsing.

## Switch NRO build

Use devkitPro MSYS2 or the GitHub Actions workflow:

```sh
pacman -S switch-dev
make
```

Optional curl support:

```sh
make USE_CURL=1
```

This local environment still lacks make/g++/devkitPro and Docker/Podman, so the Switch `.nro` build is not verified locally. FFmpeg is installed and `tools/validate_ffmpeg_transcode.py` verifies the companion transcode path locally.

## High-compatibility runtime

For maximum compatibility, run the companion server on a PC/NAS:

```sh
python companion/server.py --host 0.0.0.0 --port 8099 --allow-transcode --default-mode transcode
```

Then on Switch press `ZR` and enter:

```text
http://PC_IP:8099|auto
```

Or explicitly force transcode:

```text
http://PC_IP:8099|transcode
```

For Spider/JAR-style sources, replace the echo resolver with a real resolver and run:

```sh
python companion/server.py --host 0.0.0.0 --port 8099 --resolver-command "python companion/resolvers/echo_resolver.py"
```

Then on Switch enter:

```text
http://PC_IP:8099|resolve
```
