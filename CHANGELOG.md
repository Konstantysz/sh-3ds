# Changelog

## [Unreleased]

### Added

- `MjpegFrameSource` — live MJPEG stream via `cv::VideoCapture`; reconnect logic configurable via `CameraConfig`
- Background capture thread in `DebugLayer`; render loop no longer blocks on `Grab()`
- Live mode UI: `● LIVE` badge, grab FPS counter, frame count in Playback panel
- `--replay` CLI flag is now optional; omitting it activates live camera mode
- Camera factory dispatch in `BuildPipeline`: `mjpeg` → `MjpegFrameSource`, replay path → `FileFrameSource`/`VideoFrameSource`
- MJPEG config example documented in `config/hardware.yaml`

## [0.1.0] - 2026-03-09

### Added

- Project skeleton: CMake + vcpkg, .clang-format, .clang-tidy, .pre-commit-config
- Core types, config system (YAML), abstract pipeline interfaces, logging via Kappa
- `FileFrameSource` / `VideoFrameSource` — replay from PNG dir or video file
- `FramePreprocessor` — perspective warp, ROI extraction, dual-screen support
- `ScreenDetector` — automatic 3DS screen detection via contour + aspect-ratio classification
- `CXXStateTreeFSM` — state machine backed by CXXStateTree; detection methods: `color_histogram`, `dominant_color`, `template_match`, `intensity_event`, `always_true`
- `IntensityEventDetector` — causal Drop+Raise brightness cycle detector
- `ColorImprovement` — Gray World WB → CLAHE → gamma LUT
- `DominantColorDetector`, `HistogramDetector` — shiny detection backends
- `SoftResetStrategy`, `Orchestrator` — hunt loop with watchdog and dry-run mode
- Unified hunt config: single YAML per hunt (`config/hunts/<id>.yaml`)
- ImGui debug GUI (`SH3DSDebugApp`) with replay playback and dual-screen view
- 159 tests passing (unit + integration), including XY Starter Fennekin full-replay test

### Fixed

- `IntensityEventDetector` was advanced once per FSM candidate instead of once per frame
- `CLAHE` and gamma LUT were recreated on every frame; now cached between calls
- `ProcessDualScreen` extracted top ROIs that `ReextractRois` immediately overwrote

### Removed

- Console mode from the entry point (GUI-only; `Orchestrator` remains headless-capable)

### Known issues (deferred)

- ASAN disabled on Windows — MSVC ASAN runtime DLL not on PATH from Git Bash
