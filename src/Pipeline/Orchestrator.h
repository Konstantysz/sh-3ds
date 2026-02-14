#pragma once

#include "Capture/FramePreprocessor.h"
#include "Capture/FrameSource.h"
#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/GameStateFSM.h"
#include "Input/InputAdapter.h"
#include "Strategy/HuntStrategy.h"
#include "Vision/ShinyDetector.h"

#include <atomic>
#include <memory>

namespace SH3DS::Pipeline
{
    /**
     * @brief Main loop orchestrator. Ties together all pipeline components.
     */
    class Orchestrator
    {
    public:
        /**
         * @brief Constructs the orchestrator with all pipeline components.
         * @param frameSource Frame acquisition source.
         * @param preprocessor Perspective warp and ROI extraction.
         * @param fsm Game state tracking FSM.
         * @param detector Shiny detection engine.
         * @param strategy Hunt strategy (decides actions per state).
         * @param input Input adapter for 3DS injection.
         * @param config Runtime configuration (FPS, watchdog, dry-run).
         */
        Orchestrator(std::unique_ptr<Capture::FrameSource> frameSource,
            std::unique_ptr<Capture::FramePreprocessor> preprocessor,
            std::unique_ptr<FSM::GameStateFSM> fsm,
            std::unique_ptr<Vision::ShinyDetector> detector,
            std::unique_ptr<Strategy::HuntStrategy> strategy,
            std::unique_ptr<Input::InputAdapter> input,
            Core::OrchestratorConfig config);

        /**
         * @brief Starts the main loop. Blocks until Stop() is called.
         */
        void Run();

        /**
         * @brief Signals the main loop to stop.
         */
        void Stop();

        /**
         * @brief Returns accumulated hunt statistics.
         * @return Hunt statistics snapshot.
         */
        Core::HuntStatistics Stats() const;

    private:
        /**
         * @brief Executes one iteration of the main loop.
         */
        void MainLoopTick();

        /**
         * @brief Checks the watchdog and handles stuck states.
         */
        void HandleWatchdog();

        /**
         * @brief Executes a strategy decision (send input, alert, abort).
         * @param strategyDecision The strategy decision to execute.
         */
        void ExecuteDecision(const Strategy::StrategyDecision &strategyDecision);

        std::unique_ptr<Capture::FrameSource> frameSource;        ///< Frame acquisition source
        std::unique_ptr<Capture::FramePreprocessor> preprocessor; ///< Perspective warp and ROI extraction
        std::unique_ptr<FSM::GameStateFSM> fsm;                   ///< Game state tracking FSM
        std::unique_ptr<Vision::ShinyDetector> detector;          ///< Shiny detection engine
        std::unique_ptr<Strategy::HuntStrategy> strategy;         ///< Hunt strategy
        std::unique_ptr<Input::InputAdapter> input;               ///< Input adapter for 3DS injection
        Core::OrchestratorConfig config;                          ///< Runtime configuration
        std::atomic<bool> running = false;                        ///< Whether the main loop is running
    };
} // namespace SH3DS::Pipeline
