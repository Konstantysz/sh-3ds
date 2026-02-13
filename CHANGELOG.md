# Changelog

## [Unreleased] - v0.1.0

### Added
- Project skeleton: CMake + vcpkg, .clang-format, .clang-tidy, .pre-commit-config
- Core types: Frame, FrameMetadata, ROISet, ShinyVerdict, GameState, InputCommand
- Config system: YAML loading for hardware, game profiles, hunt strategies, detection profiles
- Logging: spdlog with console + rotating file sinks
- Abstract interfaces: FrameSource, ShinyDetector, GameStateFSM, HuntStrategy, InputAdapter
- FileFrameSource: replay frames from directory of PNGs
- FramePreprocessor: perspective warp + ROI extraction from calibrated corners
- ConfigDrivenFSM: string-based states with template match and color histogram detection
- DominantColorDetector: HSV color range shiny detection
- HistogramDetector: reference histogram comparison shiny detection
- MockInputAdapter: logs commands without sending
- SoftResetStrategy: config-driven SR loop orchestration
- Orchestrator: main loop with watchdog, dry-run mode, graceful shutdown
- YAML configs: hardware, Pokemon X/Y game profile, starter SR hunt, Froakie detection
- Unit tests: types, config loading, input encoding, FSM transitions, shiny detection, preprocessor
- Integration test: replay pipeline (preprocess -> FSM -> detector)

### Known Issues (Deferred)

- ASAN (`SH3DS_ENABLE_SANITIZERS`) disabled on Windows — MSVC ASAN runtime DLL not discoverable from Git Bash; re-enable once DLL copy logic is added to `cmake/Sanitizers.cmake`
