void PerformanceMonitor::UpdateGpuMetrics() {
    // This is a simplified placeholder implementation for GPU metrics
    // In a real implementation, you would use platform-specific APIs to get GPU usage
    // such as NVIDIA NVAPI, AMD ADL, or general purpose APIs like DirectX's IDXGIAdapter3

    // Simulated GPU usage that trends towards CPU usage (since we don't have actual GPU metrics)
    static float targetGpuUsage = 0.0f;
    static float gpuChangeRate = 0.1f;

    // Update target occasionally to simulate fluctuations
    if (rand() % 10 == 0) {
        // Random target between 0.1 and CPU usage + 0.2
        targetGpuUsage = 0.1f + (static_cast<float>(rand()) / RAND_MAX) * (m_cpuUsage + 0.2f);
        targetGpuUsage = std::min(targetGpuUsage, 1.0f); // Cap at 100%
    }

    // Gradually move towards target
    if (m_gpuUsage < targetGpuUsage) {
        m_gpuUsage += gpuChangeRate;
        if (m_gpuUsage > targetGpuUsage) m_gpuUsage = targetGpuUsage;
    }
    else if (m_gpuUsage > targetGpuUsage) {
        m_gpuUsage -= gpuChangeRate;
        if (m_gpuUsage < targetGpuUsage) m_gpuUsage = targetGpuUsage;
    }
}

bool PerformanceMonitor::IsCpuThresholdExceeded(float thresholdPercent) const {
    return (m_cpuUsage * 100.0f) > thresholdPercent;
}

bool PerformanceMonitor::IsMemoryThresholdExceeded(float thresholdMB) const {
    return GetMemoryUsageMB() > thresholdMB;
}

bool PerformanceMonitor::IsGpuThresholdExceeded(float thresholdPercent) const {
    return (m_gpuUsage * 100.0f) > thresholdPercent;
}// GameOverlay - PerformanceMonitor.cpp
// Phase 1: Foundation Framework
// Performance monitoring and metrics collection

#include "PerformanceMonitor.h"
#include <psapi.h>
#include <algorithm>
#include <numeric>

PerformanceMonitor::PerformanceMonitor() {
    // Initialize process handle for performance monitoring
    m_processHandle = GetCurrentProcess();

    // Get system timing info for CPU usage calculation
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(m_processHandle, &createTime, &exitTime, &kernelTime, &userTime)) {
        m_lastCPU.LowPart = kernelTime.dwLowDateTime;
        m_lastCPU.HighPart = kernelTime.dwHighDateTime;
        m_lastUserCPU.LowPart = userTime.dwLowDateTime;
        m_lastUserCPU.HighPart = userTime.dwHighDateTime;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_numProcessors = sysInfo.dwNumberOfProcessors;

    // Initialize frame time buffer
    std::fill(m_frameTimeBuffer.begin(), m_frameTimeBuffer.end(), 0.0f);
}

void PerformanceMonitor::BeginFrame() {
    // Record frame start time
    m_frameStart = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::EndFrame() {
    // Calculate frame time
    auto frameEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - m_frameStart);
    m_lastFrameTime = duration.count() / 1000000.0f; // Convert to seconds

    // Add to the circular buffer
    m_frameTimeBuffer[m_frameTimeBufferIndex] = m_lastFrameTime;

    // Update system metrics periodically (every 10 frames)
    static int frameCounter = 0;
    if (++frameCounter >= 10) {
        UpdateSystemMetrics();
        UpdateGpuMetrics();

        // Store in circular buffers
        m_cpuUsageBuffer[m_frameTimeBufferIndex] = m_cpuUsage;
        m_memoryUsageBuffer[m_frameTimeBufferIndex] = GetMemoryUsageMB();

        frameCounter = 0;
    }

    // Update buffer index
    m_frameTimeBufferIndex = (m_frameTimeBufferIndex + 1) % FRAME_TIME_BUFFER_SIZE;

    // Calculate average FPS over the buffer period
    float totalFrameTime = std::accumulate(m_frameTimeBuffer.begin(), m_frameTimeBuffer.end(), 0.0f);
    if (totalFrameTime > 0.0f) {
        // Count valid (non-zero) frame times
        int validFrames = std::count_if(m_frameTimeBuffer.begin(), m_frameTimeBuffer.end(),
            [](float t) { return t > 0.0f; });

        if (validFrames > 0) {
            m_framesPerSecond = static_cast<float>(validFrames) / totalFrameTime;
        }
    }
}

void PerformanceMonitor::UpdateSystemMetrics() {
    // Update CPU usage
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(m_processHandle, &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER now;
        ULARGE_INTEGER nowUser;

        now.LowPart = kernelTime.dwLowDateTime;
        now.HighPart = kernelTime.dwHighDateTime;

        nowUser.LowPart = userTime.dwLowDateTime;
        nowUser.HighPart = userTime.dwHighDateTime;

        // Calculate CPU usage
        ULARGE_INTEGER sys, user;
        sys.QuadPart = now.QuadPart - m_lastCPU.QuadPart;
        user.QuadPart = nowUser.QuadPart - m_lastUserCPU.QuadPart;

        // Calculate CPU usage as a fraction of available CPU time
        FILETIME idleTime, kernelTime2, userTime2;
        if (GetSystemTimes(&idleTime, &kernelTime2, &userTime2)) {
            ULARGE_INTEGER kT, uT;
            kT.LowPart = kernelTime2.dwLowDateTime;
            kT.HighPart = kernelTime2.dwHighDateTime;
            uT.LowPart = userTime2.dwLowDateTime;
            uT.HighPart = userTime2.dwHighDateTime;

            // Total process CPU time
            double processCpuTime = static_cast<double>(sys.QuadPart + user.QuadPart);

            // Total system CPU time (all processors)
            double systemCpuTime = static_cast<double>(kT.QuadPart + uT.QuadPart);

            // Calculate usage (accounting for multiple processors)
            if (systemCpuTime > 0) {
                m_cpuUsage = static_cast<float>(processCpuTime / systemCpuTime);
            }
        }
        else {
            // Fallback calculation if GetSystemTimes fails
            m_cpuUsage = static_cast<float>((sys.QuadPart + user.QuadPart) /
                (100.0 * m_numProcessors)); // Scale appropriately
        }

        // Store for next time
        m_lastCPU = now;
        m_lastUserCPU = nowUser;
    }

    // Update memory usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(m_processHandle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        m_memoryUsage = pmc.WorkingSetSize;
    }
}
