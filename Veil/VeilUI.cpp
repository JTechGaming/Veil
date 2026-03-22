#include "VeilUI.h"

#include <iostream>

#include "Benchmark.h"
#include "VeilConfig.h"
#include "thirdparty/imgui/imgui.h"

namespace Veil {
    void VeilUI::init(int hostIndex, VulkanDevice* device, GpuDatabase* database,
                      const std::vector<int>& weakerGpuIndices, Benchmark* benchmark, ThrottleEngine* throttleEngine) {
        m_hostIndex = hostIndex;
        m_device = device;
        m_database = database;
        m_weakerGpuIndices = weakerGpuIndices;
        m_benchmark = benchmark;
        m_throttleEngine = throttleEngine;
        
        m_currentStep = g_VeilConfig.getData().firstLaunchComplete ? Step::Calibrate : Step::FirstLaunch;
    }

    void VeilUI::render() {
        // poll benchmark and throttle state each frame
        if (m_calibrating) {
            m_calibrationProgress = m_benchmark->getProgress();
            if (m_benchmark->getComplete()) {
                m_calibrating = false;
                m_calibrated = true;
                m_hostScore = m_benchmark->getScore();
                // calculate conversion factor
                float conversionFactor = m_hostScore / m_database->get(m_hostIndex).blenderScore;
                m_conversionFactor = conversionFactor;
            }
        }
        if (m_isEmulating) {
            m_currentThrottledScore = m_throttleEngine->getCurrentScore();
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("Veil", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        switch (m_currentStep) {
        case Step::FirstLaunch: renderFirstLaunch();
            break;
        case Step::Calibrate: renderCalibrate();
            break;
        case Step::PickTarget: renderPickTarget();
            break;
        case Step::Emulate: renderEmulate();
            break;
        }

        ImGui::End();
    }
    
    void VeilUI::renderFirstLaunch() {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        float centerX = windowSize.x * 0.5f;

        // title
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("VEIL").x * 0.5f);
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "VEIL");
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("GPU Performance Emulator").x * 0.5f);
        ImGui::TextDisabled("GPU Performance Emulator");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        
        float cardWidth = windowSize.x * 0.5f;
        float cardX = centerX - cardWidth * 0.5f;

        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##telemetrycard", ImVec2(cardWidth, 360), true);

        ImGui::TextDisabled("DATA COLLECTION");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Veil can optionally collect anonymised benchmark data to improve "
            "GPU emulation accuracy over time. This includes your GPU model, "
            "VRAM, benchmark scores, and the calibration factors used during "
            "emulation. No personally identifiable information is collected. "
            "This data is used solely to improve the throttle algorithm, and "
            "other features of Veil alike. If you do opt into the telemetry  "
            "program, thank you for supporting the development of this tool! "
            "You can change this setting at any time in the settings menu.   "
        );
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("Collected: Your and the emulated GPU model, VRAM, scores, calibration factors");

        ImGui::EndChild();
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        
        float buttonWidth = 140.0f;
        float buttonHeight = 45;
        float bothButtons = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(centerX - bothButtons * 0.5f);

        if (ImGui::Button("No", ImVec2(buttonWidth, buttonHeight))) {
            g_VeilConfig.getData().telemetryEnabled = false;
            m_currentStep = Step::Calibrate;
            g_VeilConfig.save();
        }
        
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        
        if (ImGui::Button("Yes", ImVec2(buttonWidth, buttonHeight))) {
            g_VeilConfig.getData().telemetryEnabled = true;
            m_currentStep = Step::Calibrate;
            g_VeilConfig.save();
        }
        g_VeilConfig.getData().firstLaunchComplete = true;
    }

    void VeilUI::renderCalibrate() {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        float centerX = windowSize.x * 0.5f;

        // title
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("VEIL").x * 0.5f);
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "VEIL");
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("GPU Performance Emulator").x * 0.5f);
        ImGui::TextDisabled("GPU Performance Emulator");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        // GPU info card
        float cardWidth = windowSize.x * 0.5f;
        float cardX = centerX - cardWidth * 0.5f;

        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##gpucard", ImVec2(cardWidth, 150), true);

        ImGui::TextDisabled("DETECTED HARDWARE");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "%s",
                           m_device->getGpuName().c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(cardWidth - ImGui::CalcTextSize("00.0 GB VRAM").x - 8);
        ImGui::TextDisabled("%.1f GB VRAM", m_device->getVramBytes() / 1e9f);

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        // benchmark section
        if (m_calibrating) {
            ImGui::SetCursorPosX(cardX);
            ImGui::BeginChild("##benchcard", ImVec2(cardWidth, 140), true);
            ImGui::TextDisabled("RUNNING BENCHMARK");
            ImGui::Spacing();
            ImGui::ProgressBar(m_calibrationProgress, ImVec2(-1, 0));
            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::Spacing();
        }

        if (m_calibrated) {
            ImGui::SetCursorPosX(cardX);
            ImGui::BeginChild("##scorecard", ImVec2(cardWidth, 140), true);
            ImGui::TextDisabled("CALIBRATION RESULT");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f),
                               "Veil Score:  %.4f", m_hostScore);
            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::Spacing();
        }

        // buttons
        float buttonWidth = std::max(
            ImGui::CalcTextSize("Run Benchmark").x,
            ImGui::CalcTextSize("Next  >").x
        ) + 32.0f;
        ImGui::SetCursorPosX(centerX - buttonWidth * 0.5f);
        bool oldCalibrating = m_calibrating;
        if (oldCalibrating) ImGui::BeginDisabled();
        if (ImGui::Button("Run Benchmark", ImVec2(buttonWidth, 45))) {
            m_calibrated = false;
            m_calibrating = true;
            m_benchmark->run();
        }
        if (oldCalibrating) ImGui::EndDisabled();

        if (m_calibrated) {
            ImGui::SetCursorPosX(centerX - buttonWidth * 0.5f);
            if (ImGui::Button("Next  >", ImVec2(buttonWidth, 45)))
                m_currentStep = Step::PickTarget;
        }
    }

    void VeilUI::renderPickTarget() {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        float centerX = windowSize.x * 0.5f;

        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("VEIL").x * 0.5f);
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "VEIL");
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("GPU Performance Emulator").x * 0.5f);
        ImGui::TextDisabled("GPU Performance Emulator");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("Select a GPU to emulate").x * 0.5f);
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "Select a GPU to emulate");

        // GPU info card
        float cardWidth = windowSize.x * 0.5f;
        float cardX = centerX - cardWidth * 0.5f;
        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##searchcard", ImVec2(cardWidth, 120), true);
        ImGui::Text("GPU Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##gpusearchinput", m_searchBuffer, sizeof(m_searchBuffer));
        ImGui::EndChild();

        float buttonHeight = 45;
        float cardY = ImGui::GetContentRegionAvail().y - 2 * buttonHeight - 64;
        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##gpulistcard", ImVec2(cardWidth, cardY), true);

        ImGui::Text("Select GPU");
        if (ImGui::BeginListBox("##gpuselect", ImVec2(cardWidth * 0.98f, cardY * 0.95f))) {
            for (int index : m_weakerGpuIndices) {
                if (strlen(m_searchBuffer) > 0) {
                    std::string query = m_searchBuffer;
                    std::string name = m_database->get(index).name;
                    std::ranges::transform(query, query.begin(), ::tolower);
                    std::ranges::transform(name, name.begin(), ::tolower);
                    if (name.find(query) == std::string::npos)
                        continue;
                }

                const auto& entry = m_database->get(index);
                std::string label = entry.name;
                if (entry.vram.has_value()) {
                    label += "  " + std::to_string(static_cast<int>(entry.vram.value())) + "GB";
                }
                label += "  (" + std::to_string(entry.blenderScore).substr(0, 4) + ")";

                if (ImGui::Selectable(label.c_str(), m_selectedTargetIndex == index)) {
                    m_selectedTargetIndex = index;
                }
            }

            ImGui::EndListBox();
        }

        ImGui::EndChild();

        float buttonWidth = 140.0f;
        float bothButtons = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(centerX - bothButtons * 0.5f);

        if (ImGui::Button("< Back", ImVec2(buttonWidth, buttonHeight)))
            m_currentStep = Step::Calibrate;

        ImGui::SameLine();

        if (m_selectedTargetIndex == -1) ImGui::BeginDisabled();
        if (ImGui::Button("Next >", ImVec2(buttonWidth, buttonHeight)))
            if (m_selectedTargetIndex != -1) {
                m_targetScore = m_database->get(m_selectedTargetIndex).blenderScore * m_conversionFactor;
                m_currentThrottledScore = m_hostScore;
                m_targetScore = m_database->get(m_selectedTargetIndex).blenderScore * m_conversionFactor;
                if (m_database->get(m_selectedTargetIndex).vram.has_value())
                    m_vramClampGb = std::max(1.0f, m_database->get(m_selectedTargetIndex).vram.value());
                m_currentStep = Step::Emulate;
            }
        if (m_selectedTargetIndex == -1) ImGui::EndDisabled();
        
        ImGui::Spacing();
        ImGui::SetCursorPosX(centerX - buttonWidth);
        if (ImGui::Button("Skip - VRAM only", ImVec2(buttonWidth * 2, buttonHeight))) {
            m_vramOnlyMode = true;
            m_selectedTargetIndex = -1;
            m_targetScore = 0.0f;
            m_currentStep = Step::Emulate;
        }
    }

    void VeilUI::renderEmulate() {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        float centerX = windowSize.x * 0.5f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        // title
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("VEIL").x * 0.5f);
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "VEIL");
        ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("GPU Performance Emulator").x * 0.5f);
        ImGui::TextDisabled("GPU Performance Emulator");

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        // side by side cards where each takes half the center column
        float cardWidth = windowSize.x * 0.5f;
        float cardX = centerX - cardWidth * 0.5f;
        float halfCard = (cardWidth - spacing) * 0.5f;

        // host card
        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##hostcard", ImVec2(halfCard, 220), true);
        ImGui::TextDisabled("YOUR GPU");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.52f, 1.00f, 1.00f), "%s",
                           m_database->get(m_hostIndex).name.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Blender  ");
        ImGui::SameLine();
        ImGui::Text("%.4f", m_database->get(m_hostIndex).blenderScore);
        ImGui::TextDisabled("Veil     ");
        ImGui::SameLine();
        ImGui::Text("%.4f", m_hostScore);
        ImGui::EndChild();

        ImGui::SameLine();

        // target card
        ImGui::BeginChild("##targetcard", ImVec2(halfCard, 220), true);
        ImGui::TextDisabled("EMULATING");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.52f, 0.92f, 0.52f, 1.00f), "%s",
                           m_database->get(m_selectedTargetIndex).name.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Blender  ");
        ImGui::SameLine();
        ImGui::Text("%.4f", m_database->get(m_selectedTargetIndex).blenderScore);
        ImGui::TextDisabled("Veil     ");
        ImGui::SameLine();
        ImGui::Text("%.4f", m_targetScore);
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        // throttle card
        ImGui::SetCursorPosX(cardX);
        ImGui::BeginChild("##throttlecard", ImVec2(cardWidth, 400), true);
        ImGui::TextDisabled("THROTTLE");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        if (!m_vramOnlyMode) {
            float percentage = (m_throttleBaselineScore > 0.0f) ? (m_currentThrottledScore / m_throttleBaselineScore) : 0.0f;
            ImGui::ProgressBar(percentage, ImVec2(-1, 40));

            ImVec2 posMin = ImGui::GetItemRectMin();
            ImVec2 posMax = ImGui::GetItemRectMax();

            // target line
            if (m_hostScore > 0.0f) {
                float lineX = posMin.x + (posMax.x - posMin.x) * (m_targetScore / m_hostScore);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(lineX, posMin.y),
                    ImVec2(lineX, posMax.y),
                    IM_COL32(100, 255, 100, 255), 2.0f
                );
                // target label above line
                std::string targetName = m_database->get(m_selectedTargetIndex).name;
                ImVec2 targetSize = ImGui::CalcTextSize(targetName.c_str());
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(lineX - targetSize.x * 0.5f, posMin.y - targetSize.y - 2),
                    IM_COL32(100, 255, 100, 255),
                    targetName.c_str()
                );
            }

            ImGui::SetCursorScreenPos(ImVec2(posMin.x, posMax.y + ImGui::GetStyle().ItemSpacing.y));
            ImGui::TextDisabled("%.4f  /  %.4f", m_currentThrottledScore, m_hostScore);
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("VRAM CLAMP");
        ImGui::Spacing();

        float hostVramGb = m_device->getVramBytes() / 1e9f;
        float minVram = 1.0f; // red zone floor
        float maxVram = hostVramGb - 0.1f;

        // draw the progress bar background with red zone overlay
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.08f, 0.08f, 1.0f));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##vramslider", &m_vramClampGb, minVram, maxVram, "%.1f GB")) {
            m_vramClampGb = std::max(m_vramClampGb, minVram);
            if (m_isEmulating)
                m_throttleEngine->clampVram(m_device->getVramBytes(), m_vramClampGb);
        }
        ImGui::PopStyleColor();

        // red zone label
        float sliderWidth = ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x;
        float redZoneWidth = sliderWidth * (minVram / hostVramGb);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImVec2(ImGui::GetItemRectMin().x + redZoneWidth, ImGui::GetItemRectMax().y),
            IM_COL32(120, 30, 30, 120)
        );
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(ImGui::GetItemRectMin().x + 4, ImGui::GetItemRectMin().y + 2),
            IM_COL32(255, 80, 80, 200),
            "< 1GB min"
        );

        if (!m_vramClamped && m_isEmulating)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: VRAM clamp failed");
        
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        // buttons
        float buttonWidth = ImGui::CalcTextSize("Start Emulating").x * 1.1f;
        float bothButtons = buttonWidth * 2 + spacing;
        ImGui::SetCursorPosX(centerX - bothButtons * 0.5f);

        if (ImGui::Button("< Back", ImVec2(buttonWidth, 45))) {
            m_isEmulating = false;
            m_currentStep = Step::PickTarget;
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,
                              m_isEmulating
                                  ? ImVec4(0.45f, 0.15f, 0.15f, 1.0f)
                                  : ImVec4(0.20f, 0.16f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              m_isEmulating
                                  ? ImVec4(0.60f, 0.20f, 0.20f, 1.0f)
                                  : ImVec4(0.38f, 0.28f, 0.60f, 1.0f));
        if (ImGui::Button(m_isEmulating ? "Stop Emulating" : "Start Emulating",
                          ImVec2(buttonWidth, 45))) {
            m_isEmulating = !m_isEmulating;
            if (m_isEmulating) {
                m_throttleBaselineScore = m_benchmark->measureScore(); // fresh baseline
                m_throttleEngine->start(m_targetScore * (m_throttleBaselineScore / m_hostScore));
            } else {
                m_throttleEngine->stop();
            }
        }
        ImGui::PopStyleColor(2);
    }
}
