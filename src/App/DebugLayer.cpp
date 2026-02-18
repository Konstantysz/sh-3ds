#include "DebugLayer.h"

#include "Kappa/Logger.h"
#include "TextureUploader.h"

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
          playback(totalFrames, targetFps)
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

        // Process first frame
        ProcessCurrentFrame();

        LOG_INFO("DebugLayer initialized ({} frames, {:.1f} FPS)", totalFrames, targetFps);
    }

    DebugLayer::~DebugLayer()
    {
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
        bool advanced = playback.Update(deltaTime);
        if (advanced)
        {
            ProcessCurrentFrame();
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

    // TODO: move pipeline processing to a background thread for live sources (v0.2.0)
    void DebugLayer::ProcessCurrentFrame()
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

        currentRawFrame = frame->image.clone();
        rawWidth = currentRawFrame.cols;
        rawHeight = currentRawFrame.rows;

        // Upload raw frame texture
        TextureUploader::Upload(currentRawFrame, rawFrameTexture);

        // Auto-detect screen corners and update preprocessor
        if (screenDetector)
        {
            screenDetector->ApplyTo(*preprocessor, frame->image);
        }

        // Process through preprocessor
        auto dualResult = preprocessor->ProcessDualScreen(frame->image);
        if (dualResult)
        {
            if (!dualResult->warpedTop.empty())
            {
                currentTopScreen = dualResult->warpedTop;
                topWidth = currentTopScreen.cols;
                topHeight = currentTopScreen.rows;
                TextureUploader::Upload(currentTopScreen, topScreenTexture);
            }

            if (!dualResult->warpedBottom.empty())
            {
                currentBottomScreen = dualResult->warpedBottom;
                bottomWidth = currentBottomScreen.cols;
                bottomHeight = currentBottomScreen.rows;
                TextureUploader::Upload(currentBottomScreen, bottomScreenTexture);
            }

            // Update FSM
            if (dualResult->topRois)
            {
                fsm->Update(*dualResult->topRois);
                const std::string newState = fsm->CurrentState();
                if (newState != currentStateName)
                {
                    currentShinyResult = std::nullopt; // clear stale result on state change
                    currentStateName = newState;
                }
                auto ms = fsm->TimeInCurrentState();
                timeInState = static_cast<float>(ms.count()) / 1000.0f;
            }

            // Shiny detection: only run in the designated shiny-check state
            if (detector && dualResult->topRois && !shinyRoi.empty()
                && (shinyCheckState.empty() || currentStateName == shinyCheckState))
            {
                auto it = dualResult->topRois->find(shinyRoi);
                if (it != dualResult->topRois->end() && !it->second.empty())
                {
                    currentShinyResult = detector->Detect(it->second);
                }
            }
        }

        lastProcessedFrame = frameIndex;
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

        ImGui::Text("Frame: %zu / %zu", playback.GetCurrentFrameIndex() + 1, playback.GetTotalFrames());

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

        // Transport buttons
        if (ImGui::Button("<<"))
        {
            playback.StepBackward();
            ProcessCurrentFrame();
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
            ProcessCurrentFrame();
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
            ProcessCurrentFrame();
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
