# Completion Audit

Objective: build a Switch homebrew TVBox-like app that can use user-provided TVBox sources on Nintendo Switch to browse and watch videos.

## Checklist

| Requirement | Evidence | Status |
| --- | --- | --- |
| No built-in video sources | `examples/*` only use `example.invalid` and local demo files | Done |
| TVBox config support | `TvBoxConfig::loadFromLocation`, `loadFromText`; supports `sites`, `lives`, `parses` | Done |
| Local config file | `HttpClient::fetchText` reads local files; default `/switch/switchbox/config.json` | Done |
| Remote config/API support | Optional `SWITCHBOX_USE_CURL`; `make USE_CURL=1` | Implemented, not locally build-verified |
| In-app config URL entry | `App::editConfigLocation`; ZL opens software keyboard | Done |
| VOD list/search/detail | `SourceClient::fetchVodList`, `fetchVodDetail`; app uses A/Y flow | Done for supported JSON subset |
| Episode URL parsing | `parseEpisodes` handles `source$$$source` and `title$url#title$url` | Done |
| M3U live parsing | `fetchLiveChannels`, `parseM3u`; demo has 2 channels | Done |
| Favorites/history | `AppStorage`, `favorites.json`, `history.json`; UI tabs and `-` favorite | Done |
| Playback path | `PlayerBackend` uses Switch WebApplet media player on Switch | Done for WebApplet-supported URLs |
| Dedicated mpv/FFmpeg playback | Interface exists; full decoder backend not embedded | Not included |
| Host-side validation | `python tools/validate_examples.py` passes | Done |
| Switch build verification | Local environment missing devkitPro/make/compiler | Not verified locally |

## Verification performed in this environment

```text
python tools/validate_examples.py
OK: demo TVBox config, VOD response, and M3U playlist are valid
```

Toolchain check:

```text
MISS make
MISS g++
MISS aarch64-none-elf-g++
MISS pacman
```

## Remaining risk

- Remote HTTPS requires building with libcurl support and correct Switch portlibs.
- WebApplet playback can watch only formats/URLs supported by Nintendo Switch WebApplet; complex HLS variants, subtitles, soft-decoding, and unusual containers need a real mpv/FFmpeg/nxmp-style backend.
- Java/JAR Spider is intentionally unsupported because Switch homebrew is not Android.


## 2026-05-23 Compatibility Upgrade

Added high-compatibility companion playback path for "play as many TVBox sources as possible":

| Capability | Evidence | Status |
| --- | --- | --- |
| LAN companion server | `companion/server.py` | Done |
| Switch-side companion settings | `AppSettings`, `settings.json`, ZR input | Done |
| Playback URL rewrite | `AppSettings::rewritePlaybackUrl` -> `/play?mode=...&url=...` | Done |
| Proxy playback | `/play?mode=proxy` redirects to `/proxy`; Range header forwarded | Done |
| FFmpeg HLS transcode | `/play?mode=transcode` redirects to `/transcode`; ffmpeg HLS pipeline | Implemented, ffmpeg not locally end-to-end tested |
| In-app mode selection | ZR accepts `http://server:8099|auto/proxy/transcode/direct` | Done |
| Companion validation | `python tools/validate_companion.py` | Passed |
| CI validation | `.github/workflows/build-switch.yml` runs example + companion validators | Done |

Validation performed:

```text
python -m py_compile companion/server.py tools/validate_examples.py tools/validate_companion.py
python tools/validate_examples.py
python tools/validate_companion.py
```

Output:

```text
OK: demo TVBox config, VOD response, and M3U playlist are valid
OK: companion server health and play redirects are valid
```

## Updated remaining risk

- To maximize real playback compatibility, run companion with `--allow-transcode --default-mode transcode` and set Switch ZR value to `http://PC_IP:8099|transcode`.
- Java/JAR Spider sources are still not natively executable on Switch. The practical path is connecting a Java-capable resolver/plugin to the companion server.
- DRM/Widevine and paid-platform protected streams remain out of scope.
- Local machine still lacks devkitPro/make/aarch64 toolchain and Docker/Podman, so `.nro` build verification must happen in devkitPro MSYS2, WSL with devkitPro installed, or GitHub Actions.
- FFmpeg companion transcode is locally verified by `tools/validate_ffmpeg_transcode.py`.


## 2026-05-23 Resolver Plugin Upgrade

| Capability | Evidence | Status |
| --- | --- | --- |
| External resolver hook | `companion/server.py --resolver-command` | Done |
| Resolve playback mode | `/play?mode=resolve&url=...` | Done |
| Resolver protocol example | `companion/resolvers/echo_resolver.py` | Done |
| Resolver validation | `tools/validate_companion.py` starts resolver-backed server and checks redirect | Passed |

This does not bundle or implement a Java TVBox engine. It provides the integration point needed to connect one on PC/NAS.

## 2026-05-23 HLS Proxy Rewrite Upgrade

| Capability | Evidence | Status |
| --- | --- | --- |
| HLS relative segment rewrite | `rewrite_m3u8_playlist` | Done |
| HLS absolute segment rewrite | `tools/validate_companion.py` | Passed |
| HLS key URI rewrite | `tools/validate_companion.py` | Passed |

This improves compatibility for proxied M3U8 streams whose segments or keys use relative URLs.

## 2026-05-23 TVBox Header Suffix Upgrade

| Capability | Evidence | Status |
| --- | --- | --- |
| Parse `url|Header=Value` | `parse_media_spec` | Done |
| Forward headers in proxy | `handle_proxy` uses parsed extra headers | Done |
| Forward headers into FFmpeg | `ensure_transcode` adds `-headers` | Done |
| Preserve headers in HLS rewrite | `rewrite_m3u8_playlist(..., header_spec)` | Done |
| Validation coverage | `tools/validate_companion.py` checks User-Agent/Referer parsing and rewritten proxy URLs | Passed |

This improves compatibility with TVBox sources that require UA/Referer/Cookie/Authorization headers.

## 2026-05-23 Parse Fallback Upgrade

| Capability | Evidence | Status |
| --- | --- | --- |
| Detect direct-play-looking URLs | `looksDirectPlayable` in `source/App.cpp` | Done |
| Apply first global parser fallback | `applyParseUrl` in `source/App.cpp` | Done |
| Supports `{url}` parser placeholder | `applyParseUrl` | Done |

This improves compatibility with TVBox sources that require a parse API before playback.


## 2026-05-23 Host C++ Build Audit

| Capability | Evidence | Status |
| --- | --- | --- |
| Host-side C++ syntax/link check | `uvx --from ziglang python-zig c++ ... -o build/host-switchbox.exe` returned success | Passed |
| Host executable smoke run | `build/host-switchbox.exe` rendered Sources page and loaded `examples/tvbox.demo.json` | Passed |
| Switch target `.nro` build | Requires devkitPro/libnx; local machine still lacks devkitPro/make/aarch64 toolchain | Not locally verified |

Host run output included:

```text
SwitchBox Lite 0.2.0
Page: Sources | Selected: 1/1
Config: examples/tvbox.demo.json
Status: Loaded 1 sites, 1 live playlists, 1 parsers.
```


## 2026-05-23 FFmpeg Transcode Smoke Test

| Capability | Evidence | Status |
| --- | --- | --- |
| FFmpeg available on host | Installed `Gyan.FFmpeg` with winget; `ffmpeg` alias available | Done |
| Generate local sample MP4 | `tools/validate_ffmpeg_transcode.py` uses FFmpeg lavfi testsrc+sine | Passed |
| Companion `/transcode` end-to-end | Local HTTP MP4 -> FFmpeg -> HLS playlist | Passed |
| HLS playlist served | `/hls/<key>/index.m3u8` returned `#EXTM3U` and `.ts` segments | Passed |

Validation command:

```text
python tools/validate_ffmpeg_transcode.py
```

Output:

```text
OK: ffmpeg transcode smoke test produced HLS playlist
```
