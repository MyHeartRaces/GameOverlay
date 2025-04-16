// GameOverlay - UISystem.h
// Phase 3: User Interface Development
// Main UI system with tabbed interface

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "RenderSystem.h"
#include "BrowserView.h"
#include "HotkeyManager.h"
#include "PerformanceOptimizer.h"
#include "PerformanceMonitor.h"

// Forward declarations
class MainPage;
class BrowserPage;
class LinksPage;
class SettingsPage;
class HotkeySettingsPage;
class PerformanceSettingsPage;

class UISystem {
public:
    UISystem(RenderSystem* renderSystem, BrowserView* browserView, HotkeyManager* hotkeyManager,
        PerformanceOptimizer* performanceOptimizer = nullptr, PerformanceMonitor* performanceMonitor = nullptr);
    ~UISystem();

    // Disable copy and move
    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;
    UISystem(UISystem&&) = delete;
    UISystem& operator=(UISystem&&) = delete;

    // Main render method
    void Render();

    // Theme management
    enum class Theme {
        Dark,
        Light,
        Classic
    };

    void SetTheme(Theme theme);
    Theme GetTheme() const { return m_currentTheme; }

    // Get the current page to show in statusbar
    std::string GetCurrentPageName() const;

private:
    // Helper method to setup UI styling
    void ApplyTheme(Theme theme);

    // Render main layout (window, tabs, pages)
    void RenderMainLayout();

    // Render overlay statusbar
    void RenderStatusBar();

    // Tab pages
    std::unique_ptr<MainPage> m_mainPage;
    std::unique_ptr<BrowserPage> m_browserPage;
    std::unique_ptr<LinksPage> m_linksPage;
    std::unique_ptr<SettingsPage> m_settingsPage;
    std::unique_ptr<HotkeySettingsPage> m_hotkeySettingsPage;
    std::unique_ptr<PerformanceSettingsPage> m_performanceSettingsPage;

    // Current tab
    int m_currentTab = 0;

    // Set current tab
    void SetCurrentTab(int tab);
    int GetCurrentTab() const { return m_currentTab; }

    // Performance adaptations
    void AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level);

    // Theme
    Theme m_currentTheme = Theme::Dark;

    // Resource pointers (not owned)
    RenderSystem* m_renderSystem = nullptr;
    BrowserView* m_browserView = nullptr;
    HotkeyManager* m_hotkeyManager = nullptr;
    PerformanceOptimizer* m_performanceOptimizer = nullptr;
    PerformanceMonitor* m_performanceMonitor = nullptr;
};