# SH-3DS Development Plan

## v0.1.0 — Replay Pipeline (no hardware required)

**Goal:** Validate CV pipeline + FSM offline with recorded frames. TDD from day one.

**Modules:**
1. Core types (Frame, ROISet, ShinyVerdict, GameState, InputCommand)
2. Config loading (YAML: hardware, game, hunt, detection)
3. Logging (spdlog: console + rotating file)
4. FileFrameSource (read PNGs from directory)
5. FramePreprocessor (perspective warp + ROI extraction)
6. ConfigDrivenFSM (string-based states, color/template detection, debounce)
7. DominantColorDetector + HistogramDetector
8. MockInputAdapter
9. SoftResetStrategy
10. Orchestrator (main loop, watchdog, dry-run)

**Success criteria:** Replay tests pass deterministically.

### v0.1.0 Stabilization Tasks

The codebase was scaffolded in bulk and needs to be stabilized one test at a time.
Each task = make one test executable pass, commit it with the code it exercises.
Work in order — each task should fit in a single Claude Code session (~75k tokens).

**Approach:** For each task, read only the test file and its direct dependencies
(header + source). Fix compilation and test failures. Commit the working test + code.
Do not read unrelated files — token budget is tight.

**Environment note:** ASAN is disabled (`SH3DS_ENABLE_SANITIZERS=OFF` in `CMakePresets.json`).
Build & run: `cmake --build --preset debug --target <TestName>` then run the `.exe` directly.

#### Task 1: TestTypes + TestConfig (DONE)

- **Status:** PASSED (16/16 + 6/6)
- **Files:** `src/Core/Types.h`, `src/Input/InputCommand.h`, `src/Core/Config.h`, `src/Core/Config.cpp`
- **Fix applied:** Disabled ASAN in debug preset (DLL not on PATH in Git Bash)

#### Task 2: TestInputEncoding

- **Test:** `tests/unit/TestInputEncoding.cpp`
- **Likely deps:** `src/Input/InputCommand.h`, `src/Input/MockInput.cpp`, `src/Input/InputAdapter.h`
- **Goal:** Verify 20-byte packet encoding, button bitmask active-low inversion, analog/touch packing

#### Task 3: TestFramePreprocessor

- **Test:** `tests/unit/TestFramePreprocessor.cpp`
- **Likely deps:** `src/Capture/FramePreprocessor.h`, `src/Capture/FramePreprocessor.cpp`, `src/Core/Config.h`
- **Goal:** Verify perspective warp, ROI extraction from synthetic frames

#### Task 4: TestFsmTransitions

- **Test:** `tests/unit/TestFsmTransitions.cpp`
- **Likely deps:** `src/FSM/ConfigDrivenFSM.h`, `src/FSM/ConfigDrivenFSM.cpp`, `src/FSM/GameStateFSM.h`
- **Goal:** Verify state transitions, debounce logic, timeout/stuck detection

#### Task 5: TestShinyDetector

- **Test:** `tests/unit/TestShinyDetector.cpp`
- **Likely deps:** `src/Vision/ShinyDetector.h`, `src/Vision/DominantColorDetector.cpp`, `src/Vision/HistogramDetector.cpp`
- **Goal:** Verify HSV-based shiny detection on synthetic colored frames

#### Task 6: TestReplayPipeline (integration)

- **Test:** `tests/integration/TestReplayPipeline.cpp`
- **Likely deps:** All modules — this is the end-to-end pipeline test
- **Goal:** Full pipeline: FileFrameSource → Preprocessor → FSM → Detector, all wired together

#### Task 7: Main.cpp (sh3ds executable)

- **Target:** `sh3ds` executable
- **File:** `src/Main.cpp`
- **Goal:** CLI parses args, loads configs, wires pipeline, runs orchestrator in dry-run mode
- **Verify:** `sh3ds --help` works, `sh3ds --dry-run` with valid configs exits cleanly

#### Task 8: Deferred cleanup

- Re-enable ASAN: add DLL copy logic to `cmake/Sanitizers.cmake` for MSVC
- Run full `ctest --preset debug` with all tests green
- Tag `v0.1.0`

---

## v0.2.0 — Live Camera (read-only)

**Goal:** Connect to Android MJPEG stream, real-time FSM + detection, no input.

**Modules:**
- MjpegFrameSource (reconnection, frame dropping, background thread)
- FPS monitoring, latency measurement

**Success criteria:** FSM tracks manual gameplay correctly.

---

## v0.3.0 — Full Automation Loop

**Goal:** Bot sends inputs and runs SR cycles autonomously.

**Modules:**
- Luma3DSInputAdapter (20-byte UDP packets to port 4950)
- Full SoftResetStrategy integration
- Graceful SIGINT shutdown

**Success criteria:** 50+ SR cycles without human intervention.

---

## v0.4.0 — Production Bot

**Goal:** 10+ hours unattended with reliable detection.

**Modules:**
- Multi-method detection fusion
- Python scripting (pybind11)
- Shiny alert (beep, webhook)
- Metrics (JSON)

**Success criteria:** 8+ hour soak test, zero watchdog recoveries.

---

## v0.5.0+ — Future

- ML detection pipeline (CNN on sprite ROI)
- Grass/horde/Masuda hunt methods
- ORAS / USUM game profiles
- Raspberry Pi cross-compilation
- Web dashboard
