#include "DebugLayer.h"

#include "Kappa/Logger.h"
#include "TextureUploader.h"
#include "Vision/ColorImprovement.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <imgui_impl_opengl3.h>

namespace SH3DS::App
{
    DebugLayer::DebugLayer(GLFWwindow *window,
        std::unique_ptr<Capture::FrameSource> source,
        std::shared_ptr<Capture::FrameSeeker> seeker,
        std::unique_ptr<Capture::ScreenDetector> screenDetector,
        std::unique_ptr<Capture::FramePreprocessor> preprocessor,
        std::unique_ptr<FSM::GameStateFSM> fsm,
        std::unique_ptr<Vision::ShinyDetector> detector,
        std::string shinyRoi,
        std::string shinyCheckState,
        size_t totalFrames,
        float targetFps)
        : source(std::move(source)),
          seeker(seeker),
          screenDetector(std::move(screenDetector)),
          preprocessor(std::move(preprocessor)),
          fsm(std::move(fsm)),
          detector(std::move(detector)),
          shinyRoi(std::move(shinyRoi)),
          shinyCheckState(std::move(shinyCheckState)),
          playback(totalFrames, targetFps),
          isLiveSource(seeker == nullptr)
    {
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "imgui_sh3ds.ini";

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 430");

        // Create textures
        rawFrameTexture = TextureUploader::CreateTexture();
        topScreenTexture = TextureUploader::CreateTexture();
        bottomScreenTexture = TextureUploader::CreateTexture();

        if (isLiveSource)
        {
            this->source->Open();
            lastFpsTime = std::chrono::steady_clock::now();
            StartCaptureThread();
            LOG_INFO("DebugLayer initialized (LIVE mode, {:.1f} FPS target)", static_cast<double>(targetFps));
        }
        else
        {
            GrabCurrentReplayFrame();
            LOG_INFO("DebugLayer initialized ({} frames, {:.1f} FPS)", totalFrames, static_cast<double>(targetFps));
        }
    }

    DebugLayer::~DebugLayer()
    {
        // Stop capture thread before tearing down OpenGL/ImGui
        if (captureRunning)
        {
            captureRunning = false;
            if (captureThread.joinable())
            {
                captureThread.join();
            }
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (rawFrameTexture != 0)
        {
            glDeleteTextures(1, &rawFrameTexture);
        }
        if (topScreenTexture != 0)
        {
            glDeleteTextures(1, &topScreenTexture);
        }
        if (bottomScreenTexture != 0)
        {
            glDeleteTextures(1, &bottomScreenTexture);
        }
    }

    void DebugLayer::OnUpdate(float deltaTime)
    {
        if (isLiveSource)
        {
            std::optional<Core::Frame> frame;
            {
                std::lock_guard<std::mutex> lock(latestFrameMutex);
                frame = std::move(latestFrame);
                latestFrame = std::nullopt;
            }

            if (frame)
            {
                ProcessFrame(*frame);
            }

            // Update FPS estimate once per second
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - lastFpsTime).count();
            if (elapsed >= 1.0)
            {
                size_t grabbed = totalFramesGrabbed.load();
                liveGrabFps = static_cast<float>(static_cast<double>(grabbed - lastFpsCount) / elapsed);
                lastFpsCount = grabbed;
                lastFpsTime = now;
            }

            return;
        }

        bool advanced = playback.Update(deltaTime);
        if (advanced)
        {
            GrabCurrentReplayFrame();
        }
    }

    void DebugLayer::OnRender()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Full-viewport dockspace
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
                                       | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                                       | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                                       | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("SH3DSDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::End();

        // Image panels
        RenderImagePanel("Raw Camera", rawFrameTexture, rawWidth, rawHeight);
        RenderImagePanel("Top Screen", topScreenTexture, topWidth, topHeight);
        RenderImagePanel("Bottom Screen", bottomScreenTexture, bottomWidth, bottomHeight);

        // State info panel
        RenderStatePanel();

        // Playback controls
        RenderPlaybackControls();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void DebugLayer::ProcessFrame(const Core::Frame &frame)
    {
        currentRawFrame = frame.image.clone();
        rawWidth = currentRawFrame.cols;
        rawHeight = currentRawFrame.rows;

        TextureUploader::Upload(currentRawFrame, rawFrameTexture);

        if (screenDetector)
        {
            screenDetector->ApplyTo(*preprocessor, frame.image);
        }

        auto dualScreenResult = preprocessor->ProcessDualScreen(frame.image);
        if (!dualScreenResult)
        {
            return;
        }

        // Apply color correction to the full warped top image before ROI extraction so that
        // Gray World WB has the complete scene to compute balanced gains. Bottom screen is
        // LCD-rendered UI — WB correction is not applied.
        if (!dualScreenResult->warpedTop.empty())
        {
            dualScreenResult->warpedTop = Vision::ImproveFrameColors(dualScreenResult->warpedTop);
            preprocessor->ReextractRois(*dualScreenResult);
        }
        if (applyColorImprovementToDisplay && !dualScreenResult->warpedBottom.empty())
        {
            dualScreenResult->warpedBottom = Vision::ImproveFrameColors(dualScreenResult->warpedBottom);
        }

        if (!dualScreenResult->warpedTop.empty())
        {
            currentTopScreen = dualScreenResult->warpedTop;
            topWidth = currentTopScreen.cols;
            topHeight = currentTopScreen.rows;
            TextureUploader::Upload(currentTopScreen, topScreenTexture);
        }

        if (!dualScreenResult->warpedBottom.empty())
        {
            currentBottomScreen = dualScreenResult->warpedBottom;
            bottomWidth = currentBottomScreen.cols;
            bottomHeight = currentBottomScreen.rows;
            TextureUploader::Upload(currentBottomScreen, bottomScreenTexture);
        }

        if (!dualScreenResult->topRois.empty() || !dualScreenResult->bottomRois.empty())
        {
            fsm->Update(dualScreenResult->topRois, dualScreenResult->bottomRois);

            const std::string newState = fsm->GetCurrentState();
            if (newState != currentStateName)
            {
                currentShinyResult = std::nullopt; // clear stale result on state change
                currentStateName = newState;
            }

            auto ms = fsm->GetTimeInCurrentState();
            timeInState = static_cast<float>(ms.count()) / 1000.0f;
        }

        if (detector && !dualScreenResult->topRois.empty() && !shinyRoi.empty()
            && (shinyCheckState.empty() || currentStateName == shinyCheckState))
        {
            auto it = dualScreenResult->topRois.find(shinyRoi);
            if (it != dualScreenResult->topRois.end() && !it->second.empty())
            {
                currentShinyResult = detector->Detect(it->second);
            }
        }
    }

    void DebugLayer::GrabCurrentReplayFrame()
    {
        size_t frameIndex = playback.GetCurrentFrameIndex();
        if (frameIndex == lastProcessedFrame)
        {
            return;
        }

        seeker->Seek(frameIndex);

        auto frame = source->Grab();
        if (!frame)
        {
            return;
        }

        ProcessFrame(*frame);
        lastProcessedFrame = frameIndex;
    }

    void DebugLayer::StartCaptureThread()
    {
        captureRunning = true;
        captureThread = std::thread([this]() {
            while (captureRunning)
            {
                auto frame = source->Grab();
                if (frame)
                {
                    {
                        std::lock_guard<std::mutex> lock(latestFrameMutex);
                        latestFrame = std::move(frame);
                    }
                    ++totalFramesGrabbed;
                }
                else
                {
                    // Avoid busy-spin on error / stream end
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }

    void DebugLayer::RenderImagePanel(const char *title, GLuint textureId, int width, int height)
    {
        ImGui::Begin(title);

        if (textureId != 0 && width > 0 && height > 0)
        {
            ImVec2 availSize = ImGui::GetContentRegionAvail();
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            float displayWidth = availSize.x;
            float displayHeight = displayWidth / aspect;

            if (displayHeight > availSize.y)
            {
                displayHeight = availSize.y;
                displayWidth = displayHeight * aspect;
            }

            ImGui::Image(static_cast<ImTextureID>(textureId), ImVec2(displayWidth, displayHeight));
        }
        else
        {
            ImGui::TextDisabled("No image");
        }

        ImGui::End();
    }

    void DebugLayer::RenderStatePanel()
    {
        ImGui::Begin("State Info");

        if (isLiveSource)
        {
            ImGui::Text("Frames: %zu", totalFramesGrabbed.load());
        }
        else
        {
            ImGui::Text("Frame: %zu / %zu", playback.GetCurrentFrameIndex() + 1, playback.GetTotalFrames());
        }

        ImGui::Separator();

        if (ImGui::Checkbox("Color Correction (display)", &applyColorImprovementToDisplay))
        {
            lastProcessedFrame = SIZE_MAX; // force re-process with new setting
        }

        ImGui::Separator();

        ImGui::Text("FSM State: %s", currentStateName.c_str());
        ImGui::Text("Time in State: %.1f s", static_cast<double>(timeInState));

        ImGui::Separator();

        if (currentShinyResult)
        {
            const char *verdictStr = "Unknown";
            ImVec4 color(1.0f, 1.0f, 1.0f, 1.0f);

            switch (currentShinyResult->verdict)
            {
            case Core::ShinyVerdict::Shiny:
                verdictStr = "SHINY!";
                color = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);
                break;
            case Core::ShinyVerdict::NotShiny:
                verdictStr = "Not Shiny";
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                break;
            case Core::ShinyVerdict::Uncertain:
                verdictStr = "Uncertain";
                color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                break;
            }

            ImGui::TextColored(color, "Verdict: %s", verdictStr);
            ImGui::Text("Confidence: %.2f%%", currentShinyResult->confidence * 100.0);
            ImGui::Text("Method: %s", currentShinyResult->method.c_str());

            if (!currentShinyResult->details.empty())
            {
                ImGui::TextWrapped("Details: %s", currentShinyResult->details.c_str());
            }
        }
        else
        {
            ImGui::TextDisabled("No detection result");
        }

        ImGui::End();
    }

    void DebugLayer::RenderPlaybackControls()
    {
        ImGui::Begin("Playback");

        if (isLiveSource)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "● LIVE");
            ImGui::SameLine();
            ImGui::Text("  %.1f fps | %zu frames", static_cast<double>(liveGrabFps), totalFramesGrabbed.load());
            ImGui::End();
            return;
        }

        // Transport buttons
        if (ImGui::Button("<<"))
        {
            playback.StepBackward();
            GrabCurrentReplayFrame();
        }
        ImGui::SameLine();

        if (playback.IsPlaying())
        {
            if (ImGui::Button("Pause"))
            {
                playback.Pause();
            }
        }
        else
        {
            if (ImGui::Button("Play"))
            {
                playback.Play();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button(">>"))
        {
            playback.StepForward();
            GrabCurrentReplayFrame();
        }

        // Frame scrubber
        int frameIdx = static_cast<int>(playback.GetCurrentFrameIndex());
        int maxFrame = static_cast<int>(playback.GetTotalFrames()) - 1;
        if (maxFrame < 0)
        {
            maxFrame = 0;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##frame", &frameIdx, 0, maxFrame, "Frame %d"))
        {
            playback.SetFrameIndex(static_cast<size_t>(frameIdx));
            GrabCurrentReplayFrame();
        }

        // Speed control
        float speed = playback.GetPlaybackSpeed();
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 4.0f, "%.1fx"))
        {
            playback.SetPlaybackSpeed(speed);
        }
        ImGui::SameLine();
        ImGui::Text("FPS: %.1f", static_cast<double>(playback.GetTargetFps() * speed));

        ImGui::End();
    }

} // namespace SH3DS::App
