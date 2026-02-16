# SH-3DS Development Plan

## v0.1.0 — Replay Pipeline (no hardware required)

**Goal:** Validate CV pipeline + FSM offline with recorded frames. TDD from day one.

**Success criteria:** Full pipeline replays a real captured Pokemon X soft-reset cycle and produces correct FSM transitions + shiny verdicts.

### Completed

- [x] Core types, Config loading, Logging (stateless proxy)
- [x] FileFrameSource, FramePreprocessor, ConfigDrivenFSM
- [x] DominantColorDetector, HistogramDetector, TemplateMatcher
- [x] MockInputAdapter, SoftResetStrategy, Orchestrator
- [x] Unit tests pass on synthetic frames (TestTypes, TestConfig, TestInputEncoding, TestFramePreprocessor, TestFsmTransitions, TestShinyDetector)
- [x] Integration test scaffolding (TestReplayPipeline) passes on synthetic data
- [x] `sh3ds.exe --help` works

### Remaining: Real-Data Validation

The unit tests prove the code logic is correct, but v0.1.0 is not "done" until the pipeline runs successfully on **real captured footage** from an actual Pokemon X game.

#### Step 1: Capture Replay Data

Record one full soft-reset cycle from a 3DS running Pokemon X (phone camera or capture card). Extract frames as PNGs into `video_replays/xy_starter_sr/` with sorted filenames (`frame_0001.png`, `frame_0002.png`, …). The sequence should cover:

- Title screen → overworld → encounter → battle sprite visible → soft reset

Optionally, also capture a known **shiny** encounter for positive detection validation.

#### Step 2: Calibrate YAML Configs

Using the captured frames, tune the following configs to match the actual camera setup:

- `config/hardware.yaml` — screen calibration points (perspective warp corners)
- `config/games/pokemon_xy.yaml` — ROI coordinates for the actual game layout
- `config/detection/xy_froakie.yaml` — HSV ranges extracted from real normal/shiny sprites

#### Step 3: Run End-to-End

```bash
./build/debug/Debug/sh3ds.exe \
  --hardware config/hardware.yaml \
  --game config/games/pokemon_xy.yaml \
  --hunt config/hunts/xy_starter_sr.yaml \
  --detection config/detection/xy_froakie.yaml
```

Verify the logs show correct FSM state transitions and a `NotShiny` verdict (or `Shiny` if using a shiny recording).

#### Step 4: Commit Regression Fixtures

Add a small representative subset of frames (~10-20) to `tests/fixtures/xy_starter_sr/` and update `TestReplayPipeline` to run against them. This ensures future changes don't break real-data detection.

#### Step 5: Tag v0.1.0

- Run `ctest --preset debug` with all tests green (synthetic + real-data)
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
