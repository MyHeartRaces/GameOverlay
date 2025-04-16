// GameOverlay - PerformanceMonitor.h
// Phase 1: Foundation Framework
// Performance monitoring and metrics collection

#pragma once

#include <Windows.h>
#include <chrono>
#include <string>
#include <vector>
#include <array>

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor() = default;

    // Disable copy and move
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    PerformanceMonitor(PerformanceMonitor&&) = delete;
    PerformanceMonitor& operator=(PerformanceMonitor&&) = delete;

    // Frame timing methods
    void BeginFrame();
    void EndFrame();

    // Get performance metrics
    float GetFrameTime() const { return m_lastFrameTime; }
    float GetFramesPerSecond() const { return m_framesPerSecond; }

    // System resource usage
    float GetCpuUsage() const { return m_cpuUsage; }
    size_t GetMemoryUsage() const { return m_memoryUsage; }
    float GetCpuUsagePercent() const { return m_cpuUsage * 100.0f; }
    float GetMemoryUsageMB() const { return static_cast<float>(m_memoryUsage) / (1024.0f * 1024.0f); }
    float GetGpuUsage() const { return m_gpuUsage; }
    float GetGpuUsagePercent() const { return m_gpuUsage * 100.0f; }

    // Performance thresholds check
    bool IsCpuThresholdExceeded(float thresholdPercent) const;
    bool IsMemoryThresholdExceeded(float thresholdMB) const;
    bool IsGpuThresholdExceeded(float thresholdPercent) const;

    // Performance history
    const float* GetFrameTimeHistory() const { return m_frameTimeBuffer.data(); }
    const float* GetCpuUsageHistory() const { return m_cpuUsageBuffer.data(); }
    const float* GetMemoryUsageHistory() const { return m_memoryUsageBuffer.data(); }
    size_t GetHistoryBufferSize() const { return FRAME_TIME_BUFFER_SIZE; }

private:
    void UpdateSystemMetrics();
    void UpdateGpuMetrics();

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_frameStart;
    float m_lastFrameTime = 0.0f;
    float m_framesPerSecond = 0.0f;

    // FPS calculation
    static constexpr size_t FRAME_TIME_BUFFER_SIZE = 60;
    std::array<float, FRAME_TIME_BUFFER_SIZE> m_frameTimeBuffer = {};
    std::array<float, FRAME_TIME_BUFFER_SIZE> m_cpuUsageBuffer = {};
    std::array<float, FRAME_TIME_BUFFER_SIZE> m_memoryUsageBuffer = {};
    size_t m_frameTimeBufferIndex = 0;

    // System resources
    float m_cpuUsage = 0.0f;
    size_t m_memoryUsage = 0;
    float m_gpuUsage = 0.0f; // GPU usage (0.0-1.0)

    // Windows performance counters
    HANDLE m_processHandle = nullptr;
    ULARGE_INTEGER m_lastCPU = {};
    ULARGE_INTEGER m_lastSysCPU = {};
    ULARGE_INTEGER m_lastUserCPU = {};
    int m_numProcessors = 0;

    // GPU metrics tracking (simplified)
    uint64_t m_gpuQueryLastTime = 0;
    uint64_t m_gpuQueryFrequency = 0;
};