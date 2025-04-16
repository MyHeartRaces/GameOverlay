// GameOverlay - PerformanceSettingsPage.cpp
// Phase 5: Performance Optimization
// Settings page for performance configuration

#include "PerformanceSettingsPage.h"
#include "imgui.h"
#include <algorithm>

PerformanceSettingsPage::PerformanceSettingsPage(PerformanceOptimizer* optimizer, PerformanceMonitor* monitor)
    : PageBase("Performance"), m_optimizer(optimizer), m_monitor(monitor) {

    // Initialize settings from optimizer if available
    if (m_optimizer) {
        const auto& config = m_optimizer->GetConfig();

        m_settings.maxActiveFrameRate = config.maxActiveFrameRate;
        m_settings.maxInactiveFrameRate = config.maxInactiveFrameRate;
        m_settings.maxBackgroundFrameRate = config.maxBackgroundFrameRate;

        m_settings.cpuThresholdPercent = config.cpuThresholdPercent;
        m_settings.gpuThresholdPercent = config.gpuThresholdPercent;
        m_settings.memoryThresholdMB = config.memoryThresholdMB;

        m_settings.adaptiveResolution = config.adaptiveResolution;
        m_settings.throttleInactive = config.reduceInactiveQuality;
        m_settings.suspendBackground = config.suspendInactiveProcessing;
        m_settings.aggressiveMemoryCleanup = config.aggressiveMemoryCleanup;
    }

    // Initialize history arrays
    std::fill(m_cpuHistory.begin(), m_cpuHistory.end(), 0.0f);
    std::fill(m_gpuHistory.begin(), m_gpuHistory.end(), 0.0f);
    std::fill(m_memoryHistory.begin(), m_memoryHistory.end(), 0.0f);
    std::fill(m_frameTimeHistory.begin(), m_frameTimeHistory.end(), 0.0f);
}

void PerformanceSettingsPage::Render() {
    ImGui::BeginChild("PerformanceSettingsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Update history data from monitor
    if (m_monitor) {
        m_cpuHistory[m_historyIndex] = m_monitor->GetCpuUsagePercent();
        m_gpuHistory[m_historyIndex] = m_monitor->GetGpuUsagePercent();
        m_memoryHistory[m_historyIndex] = m_monitor->GetMemoryUsageMB();
        m_frameTimeHistory[m_historyIndex] = 1000.0f / m_monitor->GetFramesPerSecond(); // ms per frame

        m_historyIndex = (m_historyIndex + 1) % HISTORY_POINTS;
    }

    // Performance Overview Graphs
    RenderResourceUsageGraphs();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Performance Presets
    RenderPerformancePresets();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Frame Rate Settings
    RenderFrameRateSettings();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render Quality Settings
    RenderRenderQualitySettings();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Browser Settings
    RenderBrowserSettings();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Memory Settings
    RenderMemorySettings();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Apply Button
    if (ImGui::Button("Apply Settings", ImVec2(150, 0))) {
        ApplySettings();
        m_settingsChanged = false;
    }

    ImGui::SameLine();

    // Reset Button
    if (ImGui::Button("Reset All", ImVec2(150, 0))) {
        // Reset to default values
        m_settings = PerformanceSettings{};
        m_settingsChanged = true;
    }

    ImGui::EndChild();
}

void PerformanceSettingsPage::RenderResourceUsageGraphs() {
    RenderSectionHeader("Performance Overview");

    const float graphHeight = 80.0f;

    // CPU Usage Graph
    {
        ImGui::Text("CPU Usage: %.1f%%", m_monitor ? m_monitor->GetCpuUsagePercent() : 0.0f);
        ImGui::PlotLines("##CPUUsage", m_cpuHistory.data(), HISTORY_POINTS, m_historyIndex,
            nullptr, 0.0f, 100.0f, ImVec2(ImGui::GetContentRegionAvail().x, graphHeight));

        // CPU threshold line
        if (m_settings.cpuThresholdPercent > 0 && m_settings.cpuThresholdPercent < 100) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 p1 = ImGui::GetItemRectMin() + ImVec2(0, graphHeight * (1.0f - m_settings.cpuThresholdPercent / 100.0f));
            const ImVec2 p2 = ImVec2(ImGui::GetItemRectMax().x, p1.y);
            drawList->AddLine(p1, p2, IM_COL32(255, 0, 0, 128), 1.0f);
        }
    }

    ImGui::Spacing();

    // GPU Usage Graph
    {
        ImGui::Text("GPU Usage: %.1f%%", m_monitor ? m_monitor->GetGpuUsagePercent() : 0.0f);
        ImGui::PlotLines("##GPUUsage", m_gpuHistory.data(), HISTORY_POINTS, m_historyIndex,
            nullptr, 0.0f, 100.0f, ImVec2(ImGui::GetContentRegionAvail().x, graphHeight));

        // GPU threshold line
        if (m_settings.gpuThresholdPercent > 0 && m_settings.gpuThresholdPercent < 100) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 p1 = ImGui::GetItemRectMin() + ImVec2(0, graphHeight * (1.0f - m_settings.gpuThresholdPercent / 100.0f));
            const ImVec2 p2 = ImVec2(ImGui::GetItemRectMax().x, p1.y);
            drawList->AddLine(p1, p2, IM_COL32(255, 0, 0, 128), 1.0f);
        }
    }

    ImGui::Spacing();

    // Memory Usage Graph
    {
        ImGui::Text("Memory Usage: %.1f MB", m_monitor ? m_monitor->GetMemoryUsageMB() : 0.0f);
        ImGui::PlotLines("##MemoryUsage", m_memoryHistory.data(), HISTORY_POINTS, m_historyIndex,
            nullptr, 0.0f, 1024.0f, ImVec2(ImGui::GetContentRegionAvail().x, graphHeight));

        // Memory threshold line
        if (m_settings.memoryThresholdMB > 0 && m_settings.memoryThresholdMB < 1024) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 p1 = ImGui::GetItemRectMin() + ImVec2(0, graphHeight * (1.0f - m_settings.memoryThresholdMB / 1024.0f));
            const ImVec2 p2 = ImVec2(ImGui::GetItemRectMax().x, p1.y);
            drawList->AddLine(p1, p2, IM_COL32(255, 0, 0, 128), 1.0f);
        }
    }

    ImGui::Spacing();

    // Frame Time Graph
    {
        float currentFrameTime = m_monitor ? 1000.0f / m_monitor->GetFramesPerSecond() : 0.0f;
        ImGui::Text("Frame Time: %.2f ms (%.1f FPS)", currentFrameTime, m_monitor ? m_monitor->GetFramesPerSecond() : 0.0f);
        ImGui::PlotLines("##FrameTime", m_frameTimeHistory.data(), HISTORY_POINTS, m_historyIndex,
            nullptr, 0.0f, 33.3f, ImVec2(ImGui::GetContentRegionAvail().x, graphHeight));
    }
}

void PerformanceSettingsPage::RenderPerformancePresets() {
    RenderSectionHeader("Performance Presets");

    ImGui::TextWrapped("Choose a preset configuration based on your performance needs:");

    ImGui::Spacing();

    // Preset buttons in a single row
    const float buttonWidth = ImGui::GetContentRegionAvail().x / 4 - 8; // 4 buttons with small spacing

    if (ImGui::Button("Maximum Quality", ImVec2(buttonWidth, 0))) {
        ApplyPreset(PerformancePreset::Maximum);
        m_settingsChanged = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Balanced", ImVec2(buttonWidth, 0))) {
        ApplyPreset(PerformancePreset::Balanced);
        m_settingsChanged = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Efficiency", ImVec2(buttonWidth, 0))) {
        ApplyPreset(PerformancePreset::Efficiency);
        m_settingsChanged = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Minimal Impact", ImVec2(buttonWidth, 0))) {
        ApplyPreset(PerformancePreset::Minimal);
        m_settingsChanged = true;
    }

    ImGui::Spacing();

    // Current state information
    if (m_optimizer) {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Current State: ");
        ImGui::SameLine();

        PerformanceState state = m_optimizer->GetPerformanceState();
        ResourceUsageLevel level = m_optimizer->GetResourceUsageLevel();

        const char* stateStr = "Unknown";
        switch (state) {
        case PerformanceState::Active: stateStr = "Active"; break;
        case PerformanceState::Inactive: stateStr = "Inactive"; break;
        case PerformanceState::Background: stateStr = "Background"; break;
        case PerformanceState::LowPower: stateStr = "Low Power"; break;
        }

        const char* levelStr = "Unknown";
        switch (level) {
        case ResourceUsageLevel::Minimum: levelStr = "Minimum"; break;
        case ResourceUsageLevel::Low: levelStr = "Low"; break;
        case ResourceUsageLevel::Balanced: levelStr = "Balanced"; break;
        case ResourceUsageLevel::High: levelStr = "High"; break;
        case ResourceUsageLevel::Maximum: levelStr = "Maximum"; break;
        }

        ImGui::Text("%s / %s", stateStr, levelStr);
    }
}

void PerformanceSettingsPage::RenderFrameRateSettings() {
    RenderSectionHeader("Frame Rate Settings");

    ImGui::TextWrapped("Configure frame rate limits for different states:");

    ImGui::Spacing();

    bool changed = false;

    // Active frame rate limit
    ImGui::Text("Active state frame rate limit:");
    changed |= ImGui::SliderFloat("##ActiveFPS", &m_settings.maxActiveFrameRate, 15.0f, 240.0f, "%.0f FPS");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum FPS when the overlay is active and being used");
    }

    ImGui::Spacing();

    // Inactive frame rate limit
    ImGui::Text("Inactive state frame rate limit:");
    changed |= ImGui::SliderFloat("##InactiveFPS", &m_settings.maxInactiveFrameRate, 5.0f, 60.0f, "%.0f FPS");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum FPS when the overlay is visible but not being used");
    }

    ImGui::Spacing();

    // Background frame rate limit
    ImGui::Text("Background state frame rate limit:");
    changed |= ImGui::SliderFloat("##BackgroundFPS", &m_settings.maxBackgroundFrameRate, 1.0f, 30.0f, "%.0f FPS");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum FPS when the overlay is not visible");
    }

    ImGui::Spacing();

    // VSync option
    changed |= ImGui::Checkbox("Enable VSync", &m_settings.enableVSync);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Synchronize rendering with monitor refresh rate to reduce tearing");
    }

    if (changed) {
        m_settingsChanged = true;
    }
}

void PerformanceSettingsPage::RenderRenderQualitySettings() {
    RenderSectionHeader("Render Quality Settings");

    ImGui::TextWrapped("Configure rendering quality and optimizations:");

    ImGui::Spacing();

    bool changed = false;

    // Render scale slider
    ImGui::Text("Render Scale:");
    changed |= ImGui::SliderFloat("##RenderScale", &m_settings.renderScale, 0.25f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Scale factor for rendering resolution (lower values improve performance)");
    }

    ImGui::Spacing();

    // Adaptive resolution option
    changed |= ImGui::Checkbox("Enable Adaptive Resolution", &m_settings.adaptiveResolution);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically adjust resolution based on performance");
    }

    ImGui::Spacing();

    // Resource usage thresholds
    ImGui::Text("Resource Usage Thresholds:");

    changed |= ImGui::SliderFloat("CPU Threshold", &m_settings.cpuThresholdPercent, 40.0f, 95.0f, "%.0f%%");
    changed |= ImGui::SliderFloat("GPU Threshold", &m_settings.gpuThresholdPercent, 40.0f, 95.0f, "%.0f%%");
    changed |= ImGui::SliderFloat("Memory Threshold", &m_settings.memoryThresholdMB, 128.0f, 1024.0f, "%.0f MB");

    if (changed) {
        m_settingsChanged = true;
    }
}

void PerformanceSettingsPage::RenderBrowserSettings() {
    RenderSectionHeader("Browser Performance Settings");

    ImGui::TextWrapped("Configure browser performance optimizations:");

    ImGui::Spacing();

    bool changed = false;

    // Browser quality slider
    ImGui::Text("Browser Render Quality:");
    changed |= ImGui::SliderFloat("##BrowserQuality", &m_settings.browserQuality, 0.25f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Quality factor for browser rendering (lower values improve performance)");
    }

    ImGui::Spacing();

    // Browser throttling options
    changed |= ImGui::Checkbox("Throttle Browser When Inactive", &m_settings.throttleInactive);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reduce browser update frequency when overlay is inactive");
    }

    changed |= ImGui::Checkbox("Suspend Browser in Background", &m_settings.suspendBackground);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Completely suspend browser processing when overlay is not visible");
    }

    if (changed) {
        m_settingsChanged = true;
    }
}

void PerformanceSettingsPage::RenderMemorySettings() {
    RenderSectionHeader("Memory Management");

    ImGui::TextWrapped("Configure memory usage and cleanup:");

    ImGui::Spacing();

    bool changed = false;

    // Memory cleanup option
    changed |= ImGui::Checkbox("Aggressive Memory Cleanup", &m_settings.aggressiveMemoryCleanup);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Periodically release unused memory to reduce overall footprint");
    }

    if (changed) {
        m_settingsChanged = true;
    }

    ImGui::Spacing();

    // Memory cleanup button
    if (ImGui::Button("Release Unused Resources Now", ImVec2(250, 0))) {
        if (m_optimizer) {
            // Force a memory cleanup
            m_optimizer->SetResourceUsageLevel(ResourceUsageLevel::Minimum);
            m_optimizer->Suspend();
            m_optimizer->Resume();
        }
    }
}

void PerformanceSettingsPage::ApplySettings() {
    if (!m_optimizer) return;

    // Get config reference for modification
    auto& config = m_optimizer->GetConfig();

    // Update config from settings
    config.maxActiveFrameRate = m_settings.maxActiveFrameRate;
    config.maxInactiveFrameRate = m_settings.maxInactiveFrameRate;
    config.maxBackgroundFrameRate = m_settings.maxBackgroundFrameRate;

    config.cpuThresholdPercent = m_settings.cpuThresholdPercent;
    config.gpuThresholdPercent = m_settings.gpuThresholdPercent;
    config.memoryThresholdMB = m_settings.memoryThresholdMB;

    config.adaptiveResolution = m_settings.adaptiveResolution;
    config.reduceInactiveQuality = m_settings.throttleInactive;
    config.suspendInactiveProcessing = m_settings.suspendBackground;
    config.aggressiveMemoryCleanup = m_settings.aggressiveMemoryCleanup;

    // Apply vsync setting to render system
    if (m_optimizer) {
        // Force update to apply changes
        m_optimizer->UpdateState();
    }
}

void PerformanceSettingsPage::ApplyPreset(PerformancePreset preset) {
    switch (preset) {
    case PerformancePreset::Maximum:
        // Maximum quality, high resource usage
        m_settings.maxActiveFrameRate = 240.0f;
        m_settings.maxInactiveFrameRate = 60.0f;
        m_settings.maxBackgroundFrameRate = 30.0f;

        m_settings.cpuThresholdPercent = 90.0f;
        m_settings.gpuThresholdPercent = 90.0f;
        m_settings.memoryThresholdMB = 1024.0f;

        m_settings.renderScale = 1.0f;
        m_settings.browserQuality = 1.0f;

        m_settings.enableVSync = false;
        m_settings.adaptiveResolution = false;
        m_settings.throttleInactive = false;
        m_settings.suspendBackground = false;
        m_settings.aggressiveMemoryCleanup = false;
        break;

    case PerformancePreset::Balanced:
        // Balanced quality and performance
        m_settings.maxActiveFrameRate = 60.0f;
        m_settings.maxInactiveFrameRate = 30.0f;
        m_settings.maxBackgroundFrameRate = 10.0f;

        m_settings.cpuThresholdPercent = 80.0f;
        m_settings.gpuThresholdPercent = 80.0f;
        m_settings.memoryThresholdMB = 512.0f;

        m_settings.renderScale = 1.0f;
        m_settings.browserQuality = 1.0f;

        m_settings.enableVSync = true;
        m_settings.adaptiveResolution = true;
        m_settings.throttleInactive = true;
        m_settings.suspendBackground = false;
        m_settings.aggressiveMemoryCleanup = true;
        break;

    case PerformancePreset::Efficiency:
        // Focus on efficiency, reduced quality
        m_settings.maxActiveFrameRate = 60.0f;
        m_settings.maxInactiveFrameRate = 20.0f;
        m_settings.maxBackgroundFrameRate = 5.0f;

        m_settings.cpuThresholdPercent = 60.0f;
        m_settings.gpuThresholdPercent = 60.0f;
        m_settings.memoryThresholdMB = 256.0f;

        m_settings.renderScale = 0.75f;
        m_settings.browserQuality = 0.75f;

        m_settings.enableVSync = true;
        m_settings.adaptiveResolution = true;
        m_settings.throttleInactive = true;
        m_settings.suspendBackground = true;
        m_settings.aggressiveMemoryCleanup = true;
        break;

    case PerformancePreset::Minimal:
        // Minimum resource usage
        m_settings.maxActiveFrameRate = 30.0f;
        m_settings.maxInactiveFrameRate = 10.0f;
        m_settings.maxBackgroundFrameRate = 1.0f;

        m_settings.cpuThresholdPercent = 40.0f;
        m_settings.gpuThresholdPercent = 40.0f;
        m_settings.memoryThresholdMB = 128.0f;

        m_settings.renderScale = 0.5f;
        m_settings.browserQuality = 0.5f;

        m_settings.enableVSync = true;
        m_settings.adaptiveResolution = true;
        m_settings.throttleInactive = true;
        m_settings.suspendBackground = true;
        m_settings.aggressiveMemoryCleanup = true;
        break;
    }
}