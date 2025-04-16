// GameOverlay - PerformanceOptimizer.h
// Phase 5: Performance Optimization
// Centralized performance optimization management

#pragma once

#include <Windows.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <functional>
#include <map>

// Forward declarations
class RenderSystem;
class WindowManager;
class BrowserView;
class PerformanceMonitor;

// Performance state of the application
enum class PerformanceState {
    Active,         // User is actively using the overlay
    Inactive,       // Overlay is visible but inactive (e.g., click-through)
    Background,     // Overlay is not visible (e.g., minimized)
    LowPower        // System is in low power mode or constrained
};

// Resource usage level for various components
enum class ResourceUsageLevel {
    Minimum,        // Absolute minimum required for operation
    Low,            // Low resource usage, minimal features
    Balanced,       // Balance between performance and features
    High,           // High resource usage, all features enabled
    Maximum         // Maximum resource usage, no constraints
};

// Performance optimizer class
class PerformanceOptimizer {
public:
    PerformanceOptimizer(WindowManager* windowManager,
        RenderSystem* renderSystem,
        BrowserView* browserView,
        PerformanceMonitor* performanceMonitor);
    ~PerformanceOptimizer();

    // Disable copy and move
    PerformanceOptimizer(const PerformanceOptimizer&) = delete;
    PerformanceOptimizer& operator=(const PerformanceOptimizer&) = delete;
    PerformanceOptimizer(PerformanceOptimizer&&) = delete;
    PerformanceOptimizer& operator=(PerformanceOptimizer&&) = delete;

    // Initialize the optimizer
    void Initialize();

    // Update performance state based on current conditions
    void UpdateState();

    // Suspend/resume performance-intensive operations
    void Suspend();
    void Resume();

    // Apply frame throttling
    void ThrottleFrame();

    // Refresh rate management
    void SetTargetFrameRate(float fps);
    float GetTargetFrameRate() const;

    // Get/set resource usage levels
    void SetResourceUsageLevel(ResourceUsageLevel level);
    ResourceUsageLevel GetResourceUsageLevel() const;

    // Get current performance state
    PerformanceState GetPerformanceState() const;

    // Register a component for performance monitoring and optimization
    using OptimizationCallback = std::function<void(PerformanceState, ResourceUsageLevel)>;
    void RegisterComponent(const std::string& name, OptimizationCallback callback);

    // Static configuration options
    struct Config {
        // Frame rate limits for different states
        float maxActiveFrameRate = 60.0f;
        float maxInactiveFrameRate = 30.0f;
        float maxBackgroundFrameRate = 10.0f;

        // Resource usage thresholds
        float cpuThresholdPercent = 80.0f;
        float gpuThresholdPercent = 80.0f;
        float memoryThresholdMB = 512.0f;

        // Idle detection
        unsigned int idleTimeoutMs = 5000;

        // Background throttling
        bool enableBackgroundThrottling = true;

        // Inactive state optimizations
        bool reduceInactiveQuality = true;
        bool suspendInactiveProcessing = true;

        // Browser optimizations
        bool throttleBackgroundBrowser = true;
        bool unloadInactiveBrowser = false;

        // Render optimizations
        bool adaptiveResolution = true;
        float adaptiveResolutionMinScale = 0.5f;
        float adaptiveResolutionMaxScale = 1.0f;

        // Memory management
        bool aggressiveMemoryCleanup = true;
        unsigned int memoryCleanupIntervalMs = 60000; // 1 minute
    };

    // Get/set configuration
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }

private:
    // Helper methods for specific optimizations
    void OptimizeRenderSystem(PerformanceState state);
    void OptimizeBrowserView(PerformanceState state);
    void OptimizeMemoryUsage(PerformanceState state);
    void ScheduleBackgroundTasks();

    // Frame timing management
    void CalculateFrameDelay();
    void ApplyOptimizations();

    // Resource pointers (not owned)
    WindowManager* m_windowManager = nullptr;
    RenderSystem* m_renderSystem = nullptr;
    BrowserView* m_browserView = nullptr;
    PerformanceMonitor* m_performanceMonitor = nullptr;

    // Current state
    std::atomic<PerformanceState> m_currentState = PerformanceState::Active;
    std::atomic<ResourceUsageLevel> m_resourceUsageLevel = ResourceUsageLevel::Balanced;
    std::atomic<float> m_targetFrameRate = 60.0f;
    std::atomic<bool> m_suspended = false;

    // Frame timing
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::chrono::microseconds m_targetFrameTime = std::chrono::microseconds(16666); // 60 FPS default
    std::chrono::microseconds m_accumulatedTime = std::chrono::microseconds(0);

    // Idle tracking
    std::chrono::steady_clock::time_point m_lastActivityTime;
    bool m_isIdle = false;

    // Memory cleanup tracking
    std::chrono::steady_clock::time_point m_lastMemoryCleanupTime;

    // Thread safety
    mutable std::mutex m_mutex;

    // Registered components and their callbacks
    std::map<std::string, OptimizationCallback> m_registeredComponents;

    // Configuration
    Config m_config;

    // Render scaling
    float m_currentRenderScale = 1.0f;

    // Background task thread
    std::unique_ptr<std::thread> m_backgroundThread;
    std::atomic<bool> m_backgroundThreadRunning = false;
    void BackgroundThreadProc();
};