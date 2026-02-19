# CXXStateTree FSM Architecture

## Overview

The FSM layer uses a hybrid architecture:
- **State graph** (nodes + edges) is defined in C++ via `CXXStateTreeFSM::Builder`
- **Detection parameters** (HSV ranges, thresholds, ROI names) come from YAML via `HuntDetectionParams`
- **Hunt-specific wiring** lives in `HuntProfiles` factory methods

`CXXStateTreeFSM` implements `GameStateFSM` (abstract base, no `I` prefix).

## Key Files

| File | Role |
|------|------|
| `src/FSM/GameStateFSM.h` | Abstract interface: `Update`, `CurrentState`, `TimeInCurrentState`, `IsStuck`, `ForceState`, `Reset`, `History` |
| `src/FSM/CXXStateTreeFSM.h/.cpp` | Concrete implementation wrapping `CXXStateTree::StateTree` |
| `src/FSM/HuntProfiles.h/.cpp` | Factory: `CreateXYStarterSR(HuntDetectionParams)` → unique_ptr<CXXStateTreeFSM> |
| `external/CXXStateTree/` | External library (git submodule), target `CXXStateTree` |
| `tests/unit/TestFsmTransitions.cpp` | FSM unit tests (all green) |
| `tests/unit/TestHuntProfiles.cpp` | HuntProfiles factory tests |

## StateConfig Fields

```cpp
struct StateConfig {
    std::string id;
    std::vector<std::string> transitionsTo;  // empty → terminal state
    int maxDurationS = 60;                   // watchdog threshold for IsStuck()
    bool shinyCheck = false;                 // advisory flag (not enforced by FSM itself)
    Core::StateDetectionParams detection;    // ROI name, method, HSV bounds, pixelRatio, threshold
};
```

## How Detection Works (EvaluateRules)

1. Build candidate set from current state's reachability
2. For each candidate, look up its `detection.roi` in the `ROISet`
3. Run `color_histogram` or `template_match` → confidence score
4. Pick highest-confidence state that exceeds its `threshold`
5. Apply debounce: same state must win N consecutive frames
6. If legal and debounced: fire transition, sync `CXXStateTree` via `tree->send("goto_<target>")`

## CXXStateTree Sync

`CXXStateTree::StateTree` is an external graph that independently tracks the "legal" current state. It is synced by sending `"goto_<target>"` events when a transition fires.

**Problem**: `CXXStateTree` has no `reset()` API. After `ForceState` or a failed `send()`, the library's internal state diverges from ours.

**Solution**: `CXXStateTreeFSM::Reset()` rebuilds the entire `StateTree` from scratch using the stored `stateConfigs`. This is the same code path as `Builder::Build()` — it always produces a tree positioned at `initialState`.

```cpp
void CXXStateTreeFSM::Reset()
{
    CXXStateTree::StateTree::Builder treeBuilder;
    treeBuilder.initial(initialState);
    for (const auto &sc : stateConfigs) {
        treeBuilder.state(sc.id, [&sc](CXXStateTree::State &s) {
            for (const auto &target : sc.transitionsTo)
                s.on("goto_" + target, target);
        });
    }
    tree = std::make_unique<CXXStateTree::StateTree>(treeBuilder.build());
    currentState = initialState;
    stateEnteredAt = std::chrono::steady_clock::now();
    pendingState.clear();
    pendingFrameCount = 0;
    history.clear();
}
```

## HuntProfiles: CreateXYStarterSR

State chain (linear with `soft_reset` escape from every state):

```
load_game → game_start → cutscene_start → cutscene →
starter_pick → nickname_prompt → post_selection →
cutscene_end → party_menu → pokemon_summary → soft_reset → load_game (loop)
```

- Every state has `.transitionsTo = {"next_state", "soft_reset"}`
- `pokemon_summary` has `shinyCheck = true` (advisory flag for `SoftResetStrategy`)
- All states use `RequireState(params, "state_id")` helper — throws `std::runtime_error` with key name if YAML is missing that state

## RequireState Helper

In `HuntProfiles.cpp` anonymous namespace:

```cpp
const Core::StateDetectionParams &RequireState(
    const Core::HuntDetectionParams &params, const std::string &stateId)
{
    auto it = params.stateParams.find(stateId);
    if (it == params.stateParams.end())
        throw std::runtime_error(
            "HuntProfiles: missing detection params for required state '" + stateId
            + "'. Check fsm_states in your hunt config YAML.");
    return it->second;
}
```

This replaces `.at()` which throws `std::out_of_range` with no key name in the message.

## Detection Parameters: YAML → Struct

`UnifiedHuntConfig::fsmParams` (`HuntDetectionParams`) is loaded from the hunt YAML:

```yaml
fsm_states:
  debounce_frames: 3
  states:
    load_game:
      roi: "full_screen"
      method: "color_histogram"
      hsv_lower: [0, 0, 0]
      hsv_upper: [180, 255, 30]
      pixel_ratio_min: 0.85
      pixel_ratio_max: 1.0
      threshold: 0.7
```

`Config.cpp::ValidateStateDetectionParams()` sanitizes loaded params:
- Swapped `pixelRatioMin > pixelRatioMax` → swapped back + LOG_WARN
- Inverted HSV channels → corrected + LOG_WARN
- `threshold` outside `(0, 1]` → clamped to 0.5 + LOG_WARN
- `debounceFrames < 1` → clamped to 1 + LOG_WARN

## Test Coverage

`TestFsmTransitions.cpp` covers:
- Initial state, debounce, history recording
- `IsStuck` with `maxDurationS=0`
- Reachability filter (blocks illegal, allows legal)
- `ResetRebuildsSyncedTree` — ForceState + Reset + legal transition still fires
- `EmptyTransitionsToBlocksAllOutgoing` — terminal state stays terminal

`TestHuntProfiles.cpp` covers:
- `CreateXYStarterSR` succeeds with complete params
- `CreateXYStarterSR` throws descriptive error naming the missing state key

## Known Gaps / Future Work

- `shinyCheck` flag on `StateConfig` is set but never queried by `CXXStateTreeFSM` itself. The actual shiny-check gate is `shinyCheckState` in `UnifiedHuntConfig`, checked by `SoftResetStrategy` (console) and `DebugLayer` (GUI). The `StateConfig::shinyCheck` field is currently advisory/documentation only.
- Adding a new hunt requires a new `HuntProfiles::CreateXxx()` factory method + a corresponding YAML file under `config/hunts/`. The state names are hardcoded in the factory; detection params are YAML-driven.
- `CXXStateTree` library usage is minimal: only `send("goto_<target>")` for transition validation. The library's error-throw on illegal events provides a second validation layer on top of the reachability filter.
