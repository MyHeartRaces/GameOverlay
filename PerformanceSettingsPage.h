// GameOverlay - PerformanceSettingsPage.h
// Phase 5: Performance Optimization
// Settings page for performance configuration

#pragma once

#include "PageBase.h"
#include "PerformanceOptimizer.h"
#include "PerformanceMonitor.h"
#include <string>
#include <array>

class PerformanceSettingsPage : public PageBase {
public:
    PerformanceSettingsPage(PerformanceOptimizer* optimizer, PerformanceMonitor* monitor);
    ~PerformanceSettingsPage() = default;

    // Render performance settings page content
    void Render() override;

private:
    // Render different sections
    void RenderResourceUsageGraphs();
    void RenderPerformancePresets();
    void RenderFrameRateSettings();
    void RenderRenderQualitySettings();
    void RenderBrowserSettings();
    void RenderMemorySettings();

    // Apply settings changes
    void ApplySettings();

    // Resource pointers (not owned)
    PerformanceOptimizer* m_optimizer = nullptr;
    PerformanceMonitor* m_monitor = nullptr;

    // UI state for editing
    struct PerformanceSettings {
        // Frame rate limits
        float maxActiveFrameRate = 60.0f;
        float maxInactiveFrameRate = 30.0f;
        float maxBackgroundFrameRate = 10.0f;

        // Resource thresholds
        float cpuThresholdPercent = 80.0f;
        float gpuThresholdPercent = 80.0f;
        float memoryThresholdMB = 512.0f;

        // Quality settings
        float renderScale = 1.0f;
        float browserQuality = 1.0f;

        // Optimization options
        bool enableVSync = true;
        bool adaptiveResolution = true;
        bool throttleInactive = true;
        bool suspendBackground = true;
        bool aggressiveMemoryCleanup = true;
    };

    PerformanceSettings m_settings;
    bool m_settingsChanged = false;

    // Performance presets
    enum class PerformancePreset {
        Maximum,    // Maximum quality, high resource usage
        Balanced,   // Balanced quality and performance
        Efficiency, // Focus on efficiency, reduced quality
        Minimal     // Minimum resource usage
    };

    // CPU/GPU usage history visualization
    static constexpr int HISTORY_POINTS = 60;
    std::array<float, HISTORY_POINTS> m_cpuHistory = {};
    std::array<float, HISTORY_POINTS> m_gpuHistory = {};
    std::array<float, HISTORY_POINTS> m_memoryHistory = {};
    std::array<float, HISTORY_POINTS> m_frameTimeHistory = {};
    int m_historyIndex = 0;

    // Apply a preset configuration
    void ApplyPreset(PerformancePreset preset);
};