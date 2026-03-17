# SH-3DS

A C++20 bot for automated shiny hunting on Nintendo 3DS. It reads the console screen as a MJPEG stream, tracks game state through a YAML-configured FSM, detects shiny Pokémon by color analysis, and sends inputs via Luma3DS InputRedirection over UDP. The 3DS itself never needs to be touched.

---

## Table of contents

- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Building](#building)
- [Configuration](#configuration)
- [Running](#running)
- [Hunt profiles](#hunt-profiles)
- [Tools](#tools)
- [Testing](#testing)
- [Project layout](#project-layout)

---

## How it works

```
MJPEG stream (IP Webcam) or replay file
        │
        ▼
  FrameSource
  MjpegFrameSource | FileFrameSource | VideoFrameSource
        │
        ▼
  FramePreprocessor
  perspective warp → dual-screen split → ROI extraction → color correction
        │
        ├─────────────────────┐
        ▼                     ▼
  GameStateFSM          ShinyDetector
  CXXStateTreeFSM       DominantColorDetector | HistogramDetector
        │                     │
        └────────┬────────────┘
                 ▼
          SoftResetStrategy
                 │
                 ▼
          InputAdapter ──────────────────────────► 3DS (UDP 4950)
                 │
                 ▼
          Orchestrator  (main loop + watchdog)
```

Game state is tracked as plain strings, not enums. Adding a new game means writing a new YAML file.

Screen corners are found automatically by `ScreenDetector` through contour detection and aspect-ratio classification. No manual calibration.

Detection methods available per FSM state: `color_histogram`, `dominant_color`, `template_match`, `intensity_event` (black-screen flash detector), `always_true`.

---

## Requirements

- C++20 compiler: MSVC 2022 or GCC 12+
- CMake ≥ 3.25
- Ninja
- vcpkg with `VCPKG_ROOT` set

vcpkg dependencies (downloaded on first configure — OpenCV takes a while):

`cli11` `glfw3` `glad` `glm` `gtest` `imgui` (docking + glfw + opengl3) `nlohmann-json` `opencv4` (ffmpeg, jpeg, png) `spdlog` `yaml-cpp`

Git submodules:

```bash
git submodule update --init --recursive
```

- `external/kappa-core` — C++20 app framework (window, OpenGL, logging, layer system)
- `external/CXXStateTree` — hierarchical state machine

---

## Building

```bash
# Configure (first run downloads vcpkg deps)
cmake --preset ninja-debug

# Build
cmake --build --preset ninja-debug

# Release
cmake --preset ninja-release && cmake --build --preset ninja-release
```

Available presets:

| Preset | Generator | Type |
|---|---|---|
| `ninja-debug` | Ninja | Debug |
| `ninja-debug-asan` | Ninja | Debug + ASan/UBSan |
| `ninja-release` | Ninja | Release |
| `visual-studio` | Visual Studio 2022 | Debug / Release via IDE |
| `rpi-cross` | Ninja | Release, Raspberry Pi arm64 |

Note: ASAN is off by default on Windows (`SH3DS_ENABLE_SANITIZERS=OFF`) because the MSVC ASAN runtime DLL is not on PATH from Git Bash.

---

## Configuration

Runtime config lives in two YAML files.

### `config/hardware.yaml`

Camera, console network config, and orchestrator settings.

```yaml
camera:
  type: "mjpeg"
  uri: "http://192.168.1.x:8080/video"   # IP Webcam app on Android
  reconnect_delay_ms: 2000
  max_reconnect_attempts: 10
  grab_timeout_ms: 5000
  rotation_degrees: 90                    # 0 / 90 / 180 / 270

console:
  type: "luma3ds"
  ip: "192.168.1.x"
  port: 4950
  default_hold_ms: 120
  default_release_ms: 80

screen_calibration:
  target_width: 400    # warp destination for top screen (400×240)
  target_height: 240

orchestrator:
  target_fps: 12.0
  watchdog_timeout_s: 120
  dry_run: false       # true = no input sent, safe for observation
  log_level: "debug"
```

### `config/hunts/<hunt_id>.yaml`

One file per hunt. See [`config/hunts/xy_starter_sr_fennekin.yaml`](config/hunts/xy_starter_sr_fennekin.yaml) for a complete example.

| Key | Purpose |
|---|---|
| `hunt_id`, `hunt_name`, `target_pokemon` | Metadata |
| `screen_mode` | `"single"` or `"dual"` |
| `rois` | Named ROIs, normalized 0–1 |
| `debounce_frames` | Frames before FSM transition fires |
| `fsm_states` | Per-state detection method and params |
| `shiny_detector` | Shiny detection backend config |
| `fusion` | Decision thresholds (shiny / uncertain) |
| `actions` | Button sequences per FSM state |
| `shiny_check_state` | State where shiny check runs |
| `recovery` | What to do when stuck or detection fails |

---

## Running

```bash
# Live camera (MJPEG stream)
./build/ninja-debug/sh3ds.exe

# Replay from a video file
./build/ninja-debug/sh3ds.exe --replay path/to/replay.avi

# Replay from a directory of PNG frames
./build/ninja-debug/sh3ds.exe --replay path/to/frames/

# Custom config paths
./build/ninja-debug/sh3ds.exe \
  --hardware config/hardware.yaml \
  --hunt-config config/hunts/xy_starter_sr_fennekin.yaml

# Record the live stream while running
./build/ninja-debug/sh3ds.exe --record rec.avi
```

| Flag | Default | Description |
|---|---|---|
| `--hardware` | `config/hardware.yaml` | Hardware config |
| `--hunt-config` | `config/hunts/xy_starter_sr_fennekin.yaml` | Hunt profile |
| `--replay` | *(omit for live mode)* | Replay source: directory or video |
| `--record` | *(none)* | Record live stream to file |

The GUI opens with a dual-screen warped view. In live mode a `● LIVE` badge and grab FPS counter appear in the Playback panel.

---

## Hunt profiles

### XY starter soft reset — Fennekin

`config/hunts/xy_starter_sr_fennekin.yaml`

Resets until Fennekin is shiny. The FSM walks through:

```
load_game → game_start → cutscene_part_1 → starter_pick
          → cutscene_part_2 → game_menu → party_menu
          → pokemon_summary → [shiny check] → resetting → load_game
```

Most state transitions use `intensity_event`, which fires on a completed Drop+Raise brightness cycle (the black-screen flash between scenes). The load screen and reset state use `color_histogram`. The shiny check in `pokemon_summary` runs `DominantColorDetector` against the Pokémon sprite ROI and compares the dominant HSV to configured normal/shiny reference ranges.

When a shiny is detected the bot stops and logs the result. The `recovery` block handles stuck states (soft reset, up to 5 retries) and consecutive detection failures (skip, up to 10).

---

## Tools

Python scripts in `tools/` for offline analysis. Requires the project `venv`.

| Script | What it does |
|---|---|
| `tools/analyze_frames.py` | Runs the FSM + detectors on a replay; prints per-frame decisions |
| `tools/measure_roi_hsv_*.py` | Measures HSV statistics of a named ROI across frames — useful for tuning detection params |

---

## Testing

```bash
# Run all tests
ctest --preset ninja-debug

# Run a specific test binary
./build/ninja-debug/tests/TestTypes
./build/ninja-debug/tests/TestFsmTransitions
./build/ninja-debug/tests/TestXYStarterFennekinReplay
```

159 tests across unit and integration suites. Unit tests use synthetic `cv::Mat` frames with known HSV values as fixtures — no external images needed. Integration tests run the full pipeline on real video; the MJPEG live test is skipped when frames are absent.

---

## Project layout

```
sh-3ds/
├── config/
│   ├── hardware.yaml                       # Camera, console, orchestrator
│   └── hunts/
│       └── xy_starter_sr_fennekin.yaml     # XY Fennekin hunt profile
├── external/
│   ├── kappa-core/                         # App framework (submodule)
│   └── CXXStateTree/                       # FSM library (submodule)
├── src/
│   ├── Core/                               # Types, constants, config loader
│   ├── Input/                              # InputCommand, encoding, MockInputAdapter
│   ├── Capture/                            # FrameSource, FramePreprocessor, ScreenDetector
│   ├── FSM/                                # CXXStateTreeFSM, HuntProfiles
│   ├── Vision/                             # ShinyDetector backends, IntensityEventDetector, ColorImprovement
│   ├── Strategy/                           # SoftResetStrategy, StrategyDecision
│   ├── Pipeline/                           # Orchestrator (headless loop)
│   ├── App/                                # ImGui GUI: SH3DSDebugApp, DebugLayer, PlaybackController
│   └── Sh3DSApp/                           # main()
├── tests/
│   ├── unit/
│   └── integration/
└── tools/                                  # Python analysis scripts
```
