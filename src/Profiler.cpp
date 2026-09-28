//=====================================================================================
// Profiler.cpp
// Author: KTEngine
// Description: Real-time CPU performance profiler and ImGui visualization
//=====================================================================================

#include "Profiler.h"
#include "Renderer.h"
#include "Manager.h"
#include "imgui.h"
#include <algorithm>
#include <numeric>
#include <cstdio>

namespace {
    constexpr int HISTORY_SIZE = 180; // ~3 seconds at 60 FPS

    std::chrono::high_resolution_clock::time_point s_frameStartTime;
    float s_lastFrameTime = 16.6f;
    float s_fps = 60.0f;

    float s_frameTimeHistory[HISTORY_SIZE] = { 0 };
    int s_historyOffset = 0;

    // Measurement per stage (Tag -> milliseconds)
    std::unordered_map<std::string, float> s_currentFrameSamples;
    std::unordered_map<std::string, float> s_lastFrameSamples;
    std::vector<std::string> s_stageOrder;

    // Simulation & Rendering Metrics
    PhysicsMetrics s_currentPhysicsMetrics;
    PhysicsMetrics s_lastPhysicsMetrics;

    RenderMetrics s_currentRenderMetrics;
    RenderMetrics s_lastRenderMetrics;

    bool s_freeze = false; // Freeze graph updates
    float s_maxPlotScale = 35.0f; // Graph Y-max (33.3ms = 30fps)

    std::string FormatWithCommas(int value) {
        std::string s = std::to_string(value);
        int start = (value < 0) ? 1 : 0;
        for (int i = (int)s.length() - 3; i > start; i -= 3) {
            s.insert(i, ",");
        }
        return s;
    }
}

void Profiler::BeginFrame() {
    s_frameStartTime = std::chrono::high_resolution_clock::now();
    s_currentFrameSamples.clear();
    s_currentRenderMetrics = RenderMetrics();
}

void Profiler::EndFrame() {
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(frameEndTime - s_frameStartTime).count();

    if (!s_freeze) {
        s_lastFrameTime = elapsedMs;
        s_fps = (elapsedMs > 0.001f) ? (1000.0f / elapsedMs) : 0.0f;

        s_frameTimeHistory[s_historyOffset] = elapsedMs;
        s_historyOffset = (s_historyOffset + 1) % HISTORY_SIZE;

        s_lastFrameSamples = s_currentFrameSamples;
        s_lastPhysicsMetrics = s_currentPhysicsMetrics;
        s_lastRenderMetrics = s_currentRenderMetrics;
    }
}

void Profiler::RecordCPUTime(const char* tag, float milliseconds) {
    if (s_freeze) return;

    if (s_currentFrameSamples.find(tag) == s_currentFrameSamples.end()) {
        s_currentFrameSamples[tag] = milliseconds;
        // Preserve registration order
        if (std::find(s_stageOrder.begin(), s_stageOrder.end(), tag) == s_stageOrder.end()) {
            s_stageOrder.push_back(tag);
        }
    } else {
        s_currentFrameSamples[tag] += milliseconds;
    }
}

void Profiler::RecordPhysicsMetrics(const PhysicsMetrics& metrics) {
    if (s_freeze) return;
    s_currentPhysicsMetrics = metrics;
}

void Profiler::RecordDrawCall(int vertexCount, int indexCount) {
    if (s_freeze) return;
    s_currentRenderMetrics.drawCalls++;
    s_currentRenderMetrics.totalVertices += vertexCount;
    s_currentRenderMetrics.totalTriangles += (indexCount > 0) ? (indexCount / 3) : (vertexCount / 3);
}

void Profiler::RecordSceneObjects(int totalObjs, int activeObjs) {
    if (s_freeze) return;
    s_currentRenderMetrics.totalGameObjects = totalObjs;
    s_currentRenderMetrics.activeGameObjects = activeObjs;
}

float Profiler::GetLastFrameTime() {
    return s_lastFrameTime;
}

float Profiler::GetFPS() {
    return s_fps;
}

const PhysicsMetrics& Profiler::GetLastPhysicsMetrics() {
    return s_lastPhysicsMetrics;
}

const RenderMetrics& Profiler::GetLastRenderMetrics() {
    return s_lastRenderMetrics;
}

void Profiler::RenderUI(bool* pOpen) {
    if (pOpen && !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(440, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler", pOpen)) {
        ImGui::End();
        return;
    }

    // Compute statistics
    float avgTime = 0.0f;
    float minTime = 9999.0f;
    float maxTime = 0.0f;
    int validCount = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        float val = s_frameTimeHistory[i];
        if (val > 0.0f) {
            avgTime += val;
            if (val < minTime) minTime = val;
            if (val > maxTime) maxTime = val;
            validCount++;
        }
    }
    if (validCount > 0) {
        avgTime /= (float)validCount;
    }
    if (minTime > 9000.0f) minTime = 0.0f;

    // =========================================================================
    // 1. Frame Overview
    // =========================================================================
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Overview");
    ImGui::Separator();

    // FPS Display (Green for >=55, Yellow for >=30, Red for <30)
    ImVec4 fpsColor = (s_fps >= 55.0f) ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                      (s_fps >= 30.0f) ? ImVec4(1.0f, 0.9f, 0.2f, 1.0f) :
                                         ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

    ImGui::Text("FPS: ");
    ImGui::SameLine();
    ImGui::TextColored(fpsColor, "%.1f", s_fps);
    ImGui::SameLine(140);
    ImGui::Text("Frame Time: %.2f ms", s_lastFrameTime);

    ImGui::Text("Avg: %.2f ms  |  Min: %.2f ms  |  Max: %.2f ms", avgTime, minTime, maxTime);

    // Controls: Pause and Scale
    ImGui::Checkbox("Pause Graph", &s_freeze);
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Scale Max", &s_maxPlotScale, 20.0f, 100.0f, "%.0f ms");

    // Frame Time Graph (PlotLines)
    char overlayText[64];
    sprintf_s(overlayText, "%.2f ms (60fps=16.6ms)", s_lastFrameTime);
    ImGui::PlotLines("##FrameTimePlot", s_frameTimeHistory, HISTORY_SIZE, s_historyOffset,
                     overlayText, 0.0f, s_maxPlotScale, ImVec2(-1, 85));

    ImGui::Spacing();

    // =========================================================================
    // 2. CPU Time Breakdown
    // =========================================================================
    if (ImGui::CollapsingHeader("CPU Time Breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
        float recordedTotal = 0.0f;
        for (const auto& stage : s_stageOrder) {
            auto it = s_lastFrameSamples.find(stage);
            if (it != s_lastFrameSamples.end()) {
                recordedTotal += it->second;
            }
        }

        ImGui::Text("Measured Total: %.2f ms", recordedTotal);
        ImGui::Spacing();

        // Color palette for stages
        const ImVec4 colors[6] = {
            ImVec4(0.2f, 0.8f, 0.3f, 1.0f), // Green (Physics)
            ImVec4(0.3f, 0.6f, 1.0f, 1.0f), // Blue (Logic)
            ImVec4(1.0f, 0.6f, 0.1f, 1.0f), // Orange (Render)
            ImVec4(0.8f, 0.4f, 0.9f, 1.0f), // Purple (ImGui)
            ImVec4(0.3f, 0.9f, 0.9f, 1.0f), // Cyan
            ImVec4(0.9f, 0.8f, 0.2f, 1.0f)  // Yellow
        };

        int colorIdx = 0;
        for (const auto& stage : s_stageOrder) {
            float stageMs = 0.0f;
            auto it = s_lastFrameSamples.find(stage);
            if (it != s_lastFrameSamples.end()) {
                stageMs = it->second;
            }

            float ratio = (s_lastFrameTime > 0.001f) ? (stageMs / s_lastFrameTime) : 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;

            ImVec4 col = colors[colorIdx % 6];
            colorIdx++;

            // Stage name and duration
            ImGui::TextColored(col, "%-16s", stage.c_str());
            ImGui::SameLine(160);
            ImGui::Text("%6.2f ms  (%4.1f%%)", stageMs, ratio * 100.0f);

            // Progress bar
            char barOverlay[32];
            sprintf_s(barOverlay, "%.2f ms", stageMs);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
            ImGui::ProgressBar(ratio, ImVec2(-1, 14), barOverlay);
            ImGui::PopStyleColor();
        }

        // Unmeasured / Remaining frame time
        float unmeasuredMs = s_lastFrameTime - recordedTotal;
        if (unmeasuredMs > 0.01f) {
            float unmeasuredRatio = (s_lastFrameTime > 0.001f) ? (unmeasuredMs / s_lastFrameTime) : 0.0f;
            if (unmeasuredRatio > 1.0f) unmeasuredRatio = 1.0f;

            ImVec4 otherCol = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            ImGui::TextColored(otherCol, "%-16s", "Other / Wait");
            ImGui::SameLine(160);
            ImGui::Text("%6.2f ms  (%4.1f%%)", unmeasuredMs, unmeasuredRatio * 100.0f);

            char barOverlay[32];
            sprintf_s(barOverlay, "%.2f ms", unmeasuredMs);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, otherCol);
            ImGui::ProgressBar(unmeasuredRatio, ImVec2(-1, 14), barOverlay);
            ImGui::PopStyleColor();
        }
    }

    ImGui::Spacing();

    // =========================================================================
    // 3. Physics Metrics (Phase 2)
    // =========================================================================
    if (ImGui::CollapsingHeader("Physics Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Collision Wireframes", &Manager::GetShowColliderWireframe());
        ImGui::Spacing();

        int inactiveRBs = s_lastPhysicsMetrics.totalRigidBodies - s_lastPhysicsMetrics.activeRigidBodies;
        int inactiveCols = s_lastPhysicsMetrics.totalColliders - s_lastPhysicsMetrics.activeColliders;

        ImGui::Columns(2, "PhysicsMetricsCol", false);
        ImGui::SetColumnWidth(0, 180.0f);

        ImGui::Text("RigidBodies:");
        ImGui::NextColumn();
        ImGui::Text("%d active / %d total (%d idle)",
                    s_lastPhysicsMetrics.activeRigidBodies,
                    s_lastPhysicsMetrics.totalRigidBodies,
                    inactiveRBs);
        ImGui::NextColumn();

        ImGui::Text("Colliders:");
        ImGui::NextColumn();
        ImGui::Text("%d active / %d total (%d inactive)",
                    s_lastPhysicsMetrics.activeColliders,
                    s_lastPhysicsMetrics.totalColliders,
                    inactiveCols);
        ImGui::NextColumn();

        ImGui::Text("Broadphase Pairs:");
        ImGui::NextColumn();
        ImGui::Text("%s tested pairs", FormatWithCommas(s_lastPhysicsMetrics.broadphasePairs).c_str());
        ImGui::NextColumn();

        ImGui::Text("Narrowphase Tests:");
        ImGui::NextColumn();
        ImGui::Text("%s geometric tests", FormatWithCommas(s_lastPhysicsMetrics.narrowphaseTests).c_str());
        ImGui::NextColumn();

        ImGui::Text("Contact Manifolds:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%d collisions active", s_lastPhysicsMetrics.contactManifolds);
        ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // =========================================================================
    // 4. Rendering Metrics (Phase 2)
    // =========================================================================
    if (ImGui::CollapsingHeader("Rendering Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
        GraphicsAPI currentAPI = Renderer::GetGraphicsAPI();
        const char* apiName = (currentAPI == GraphicsAPI::DirectX12) ? "DirectX 12" : "DirectX 11";

        ImGui::Columns(2, "RenderMetricsCol", false);
        ImGui::SetColumnWidth(0, 180.0f);

        ImGui::Text("Graphics API:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", apiName);
        ImGui::NextColumn();

        ImGui::Text("Draw Calls:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%d calls / frame", s_lastRenderMetrics.drawCalls);
        ImGui::NextColumn();

        ImGui::Text("Triangles (Polys):");
        ImGui::NextColumn();
        ImGui::Text("%s triangles", FormatWithCommas(s_lastRenderMetrics.totalTriangles).c_str());
        ImGui::NextColumn();

        ImGui::Text("Vertices:");
        ImGui::NextColumn();
        ImGui::Text("%s vertices", FormatWithCommas(s_lastRenderMetrics.totalVertices).c_str());
        ImGui::NextColumn();

        ImGui::Text("Scene GameObjects:");
        ImGui::NextColumn();
        ImGui::Text("%d active / %d total", s_lastRenderMetrics.activeGameObjects, s_lastRenderMetrics.totalGameObjects);
        ImGui::NextColumn();

        ImGui::Text("Scene Buffer Resolution:");
        ImGui::NextColumn();
        ImGui::Text("%.0f x %.0f", Renderer::GetSceneWidth(), Renderer::GetSceneHeight());
        ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // =========================================================================
    // 5. Budget Guide
    // =========================================================================
    if (ImGui::CollapsingHeader("Frame Budget Guide")) {
        float budget60 = 16.66f;
        float budget30 = 33.33f;

        float rem60 = budget60 - s_lastFrameTime;
        float rem30 = budget30 - s_lastFrameTime;

        ImGui::Text("60 FPS Budget (16.6 ms):");
        ImGui::SameLine(220);
        if (rem60 >= 0.0f) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "+%.2f ms available", rem60);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.2f ms exceeded", -rem60);
        }

        ImGui::Text("30 FPS Budget (33.3 ms):");
        ImGui::SameLine(220);
        if (rem30 >= 0.0f) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "+%.2f ms available", rem30);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.2f ms exceeded", -rem30);
        }
    }

    ImGui::End();
}
