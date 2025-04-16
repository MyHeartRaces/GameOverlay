void UISystem::AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level) {
    // Adjust UI based on performance state
    if (state == PerformanceState::LowPower || state == PerformanceState::Background) {
        // Reduce UI complexity in background or low power states
        // This is a placeholder - real implementation would do more
    }
}// GameOverlay - UISystem.cpp
// Phase 3: User Interface Development
// Main UI system with tabbed interface

#include "UISystem.h"
#include "imgui.h"
#include "MainPage.h"
#include "BrowserPage.h"
#include "LinksPage.h"
#include "SettingsPage.h"
#include "HotkeySettingsPage.h"
#include "PerformanceSettingsPage.h"
#include <string>

UISystem::UISystem(RenderSystem* renderSystem, BrowserView* browserView, HotkeyManager* hotkeyManager,
    PerformanceOptimizer* performanceOptimizer, PerformanceMonitor* performanceMonitor)
    : m_renderSystem(renderSystem), m_browserView(browserView), m_hotkeyManager(hotkeyManager),
    m_performanceOptimizer(performanceOptimizer), m_performanceMonitor(performanceMonitor) {

    // Create pages
    m_mainPage = std::make_unique<MainPage>();
    m_browserPage = std::make_unique<BrowserPage>(m_browserView);
    m_linksPage = std::make_unique<LinksPage>();
    m_settingsPage = std::make_unique<SettingsPage>(this);
    m_hotkeySettingsPage = std::make_unique<HotkeySettingsPage>(m_hotkeyManager);

    // Create performance settings page if optimizer and monitor are available
    if (m_performanceOptimizer && m_performanceMonitor) {
        m_performanceSettingsPage = std::make_unique<PerformanceSettingsPage>(m_performanceOptimizer, m_performanceMonitor);
    }

    // Set initial theme
    ApplyTheme(m_currentTheme);

    // Register tab switching hotkeys
    if (m_hotkeyManager) {
        // Update the show_main action to switch tab
        m_hotkeyManager->RegisterHotkey("show_main", Hotkey('1', false, true), [this]() {
            SetCurrentTab(0); // Switch to Main tab
            });

        // Update the show_browser action to switch tab
        m_hotkeyManager->RegisterHotkey("show_browser", Hotkey('2', false, true), [this]() {
            SetCurrentTab(1); // Switch to Browser tab
            });

        // Update the show_links action to switch tab
        m_hotkeyManager->RegisterHotkey("show_links", Hotkey('3', false, true), [this]() {
            SetCurrentTab(2); // Switch to Links tab
            });

        // Update the show_settings action to switch tab
        m_hotkeyManager->RegisterHotkey("show_settings", Hotkey('4', false, true), [this]() {
            SetCurrentTab(3); // Switch to Settings tab
            });

        // Add hotkey for hotkey settings tab
        m_hotkeyManager->RegisterHotkey("show_hotkeys", Hotkey('5', false, true), [this]() {
            SetCurrentTab(4); // Switch to Hotkeys tab
            });

        // Add hotkey for performance settings tab if available
        if (m_performanceSettingsPage) {
            m_hotkeyManager->RegisterHotkey("show_performance", Hotkey('6', false, true), [this]() {
                SetCurrentTab(5); // Switch to Performance tab
                });
        }
    }
}

UISystem::~UISystem() {
    // Pages are automatically cleaned up by unique_ptr
}

void UISystem::Render() {
    // Render main layout with tabbed interface
    RenderMainLayout();

    // Render status bar
    RenderStatusBar();
}

void UISystem::SetTheme(Theme theme) {
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        ApplyTheme(theme);
    }
}

std::string UISystem::GetCurrentPageName() const {
    switch (m_currentTab) {
    case 0: return "Main";
    case 1: return "Browser";
    case 2: return "Links";
    case 3: return "Settings";
    case 4: return "Hotkeys";
    case 5: return "Performance";
    default: return "Unknown";
    }
}

void UISystem::SetCurrentTab(int tab) {
    // Validate tab index
    int maxTab = m_performanceSettingsPage ? 5 : 4;
    if (tab >= 0 && tab <= maxTab) {
        m_currentTab = tab;
    }
}

void UISystem::ApplyTheme(Theme theme) {
    ImGuiStyle& style = ImGui::GetStyle();

    // Reset to default values
    style = ImGuiStyle();

    // Common styling for all themes
    style.FrameRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.ScrollbarSize = 16;
    style.GrabMinSize = 8;

    // Apply theme-specific colors
    switch (theme) {
    case Theme::Dark: {
        ImGui::StyleColorsDark();
        // Custom adjustments to dark theme
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.94f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.11f, 0.15f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.32f, 0.63f, 1.00f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.22f, 0.86f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.26f, 0.48f, 0.80f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.32f, 0.32f, 0.63f, 1.00f);
        break;
    }
    case Theme::Light: {
        ImGui::StyleColorsLight();
        // Custom adjustments to light theme
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.78f, 0.90f, 1.00f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.75f, 0.75f, 0.75f, 0.86f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.66f, 0.66f, 0.80f, 0.80f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.68f, 0.68f, 0.90f, 1.00f);
        break;
    }
    case Theme::Classic: {
        ImGui::StyleColorsClassic();
        // Custom adjustments to classic theme
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.26f, 0.35f, 0.41f, 1.00f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.25f, 0.30f, 0.86f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.35f, 0.41f, 0.80f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.38f, 0.47f, 1.00f);
        break;
    }
    }
}

void UISystem::RenderMainLayout() {
    // Main window flags
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Set main window position and size
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 50, viewport->WorkPos.y + 50));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - 100, viewport->WorkSize.y - 100));

    // Begin main window
    if (ImGui::Begin("GameOverlay", nullptr, windowFlags)) {
        // Create tabs
        if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Main")) {
                m_currentTab = 0;
                m_mainPage->Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Browser")) {
                m_currentTab = 1;
                m_browserPage->Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Links")) {
                m_currentTab = 2;
                m_linksPage->Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings")) {
                m_currentTab = 3;
                m_settingsPage->Render();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hotkeys")) {
                m_currentTab = 4;
                m_hotkeySettingsPage->Render();
                ImGui::EndTabItem();
            }

            if (m_performanceSettingsPage && ImGui::BeginTabItem("Performance")) {
                m_currentTab = 5;
                m_performanceSettingsPage->Render();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void UISystem::RenderStatusBar() {
    // Status bar at the bottom of the screen
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - 30));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 30));
    ImGui::SetNextWindowBgAlpha(0.6f);

    ImGuiWindowFlags statusFlags =
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("StatusBar", nullptr, statusFlags)) {
        // Current page indicator
        ImGui::Text("Current Page: %s", GetCurrentPageName().c_str());

        // FPS counter on the right
        float fps = ImGui::GetIO().Framerate;
        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::Text("%.1f FPS (%.3f ms)", fps, 1000.0f / fps);
    }
    ImGui::End();
}
