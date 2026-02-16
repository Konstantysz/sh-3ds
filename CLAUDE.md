# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (first run downloads vcpkg deps — OpenCV takes 10-30 min)
cmake --preset debug

# Build
cmake --build --preset debug

# Run all tests
ctest --preset debug

# Run a single test executable
./build/debug/tests/TestTypes        # or any test target name

# Release build
cmake --preset release && cmake --build --preset release
```

Requires `VCPKG_ROOT` environment variable pointing to vcpkg installation.

## Coding Standards

**Naming:** PascalCase for classes, structs, functions, enums. camelCase for ALL variables including members — no trailing underscores. No `I` prefix on interfaces (`FrameSource`, not `IFrameSource`).

**Files:** Headers are `PascalCase.h`, sources are `PascalCase.cpp`, tests are `TestPascalCase.cpp`. Config files are `snake_case.yaml`.

**Style:** Allman braces, 4-space indent, 120-char line limit, right-aligned pointers (`Type *ptr`). Function definition order in `.cpp` must match declaration order in `.h`.

**C++20:** Use concepts, ranges, designated initializers, `std::span`. OpenCV headers use `.hpp` extension (`<opencv2/core.hpp>`).

**Include order** (enforced by .clang-format): local project headers → sh3ds headers → third-party (opencv, spdlog, yaml-cpp, CLI) → standard library. Each group separated by blank line.

**TDD:** Red → Green → Refactor. Write GTest first, watch it fail, then implement.

## Architecture

Vision-driven bot pipeline — all game logic is config-driven (YAML), no hardcoded magic numbers:

```
FrameSource (abstract) → FramePreprocessor → GameStateFSM (abstract) → ShinyDetector (abstract)
                                                    ↓                          ↓
                                              HuntStrategy (abstract) ← ← ← ←
                                                    ↓
                                              InputAdapter (abstract) → 3DS (UDP:4950)
                                                    ↓
                                              Orchestrator (main loop, watchdog)
```

**Abstract bases** (no `I` prefix) in `include/sh3ds/`:

- `FrameSource` — frame acquisition (`FileFrameSource` for replay, `MjpegFrameSource` planned)
- `GameStateFSM` — game state tracking (`ConfigDrivenFSM` loads states/rules from YAML)
- `ShinyDetector` — shiny detection (`DominantColorDetector`, `HistogramDetector`)
- `HuntStrategy` — hunt orchestration (`SoftResetStrategy`)
- `InputAdapter` — 3DS input injection (`MockInputAdapter` for testing)

**Logging:** Uses `Kappa::Logger` macros like `LOG_ERROR` or `LOG_INFO`.

**Game states are strings** (not enums) loaded from YAML — new games = new config, no recompilation. FSM uses debounce (N consecutive frames) before transitioning.

**Config hierarchy:** `config/hardware.yaml` (camera, console, screen calibration) → `config/games/*.yaml` (ROIs, FSM states) → `config/hunts/*.yaml` (input sequences per state) → `config/detection/*.yaml` (HSV ranges, histogram refs, fusion weights).

## Key Constants

3DS screen resolutions are defined in `src/Core/Constants.h`:

- Top screen: 400x240 (5:3), bottom screen: 320x240 (4:3)
- Use `Core::kTopScreenWidth`, `Core::kBottomScreenAspectRatio`, etc. — never hardcode these values

Hardware constants:

- Luma3DS InputRedirection: UDP port 4950, 20-byte packets, active-low buttons
- Soft reset combo: `L|R|START = 0x0308`
- Button bitmask: A=0x001, B=0x002, SELECT=0x004, START=0x008, R=0x100, L=0x200

## Test Structure

Tests use `sh3ds_add_test(name source)` helper in `tests/CMakeLists.txt`. Test names match `TestPascalCase` convention. Synthetic `cv::Mat` frames with known HSV colors used as fixtures — no external image files required for unit tests.

## Release Versioning

Each milestone is a pre-release: v0.1.0 (replay pipeline), v0.2.0 (live camera), v0.3.0 (automation loop), v0.4.0 (production bot). Update `CHANGELOG.md` with each release.
