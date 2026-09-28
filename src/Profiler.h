//=====================================================================================
// Profiler.h
// Author: KTEngine
// Description: Real-time CPU performance profiler and ImGui visualization
//=====================================================================================

#pragma once

#ifndef KT_PROFILER_H_
#define KT_PROFILER_H_

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>

class Profiler {
public:
    static void BeginFrame();
    static void EndFrame();

    static void RecordCPUTime(const char* tag, float milliseconds);

    static void RenderUI(bool* pOpen);

    // Get profiling data
    static float GetLastFrameTime();
    static float GetFPS();
};

// RAII Scope Timer
class ProfileScope {
private:
    const char* m_name;
    std::chrono::high_resolution_clock::time_point m_startTime;
public:
    explicit ProfileScope(const char* name)
        : m_name(name), m_startTime(std::chrono::high_resolution_clock::now()) {}

    ~ProfileScope() {
        auto endTime = std::chrono::high_resolution_clock::now();
        float ms = std::chrono::duration<float, std::milli>(endTime - m_startTime).count();
        Profiler::RecordCPUTime(m_name, ms);
    }
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) ProfileScope PROFILE_CONCAT(scope_profiler_, __LINE__)(name)

#endif // !KT_PROFILER_H_
