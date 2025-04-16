// GameOverlay - SettingsPage.h
// Phase 3: User Interface Development
// Settings page for application configuration

#pragma once

#include "PageBase.h"
#include <string>
#include <functional>
#include <map>

// Forward declaration
class UISystem;

class SettingsPage : public PageBase {
public:
    SettingsPage(UISystem* uiSystem);
    ~SettingsPage() = default;

    // Render settings page content
    void Render() override;

private:
    // Render different settings sections
    void RenderGeneralSettings();
    void RenderBrowserSettings();
    void RenderAppearanceSettings();
    void RenderHotkeySettings();
    void RenderAboutSection();

    // Apply settings changes
    void ApplyGeneralSettings();
    void ApplyBrowserSettings();
    void ApplyAppearanceSettings();
    void ApplyHotkeySettings();

    // Settings data
    struct GeneralSettings {
        bool startWithWindows = false;
        bool startMinimized = false;
        bool checkForUpdates = true;
        int inactiveOpacity = 50;
        bool autoHide = false;
        int autoHideDelay = 5;
    } m_generalSettings;

    struct BrowserSettings {
        bool enableJavaScript = true;
        bool enablePlugins = true;
        bool enableCookies = true;
        bool clearCacheOnExit = false;
        bool clearHistoryOnExit = false;
        std::string homePage = "https://www.google.com";
        std::string searchEngine = "Google";
    } m_browserSettings;

    struct AppearanceSettings {
        int theme = 0;
        float fontSize = 1.0f;
        int windowWidth = 1280;
        int windowHeight = 720;
        bool useCustomColors = false;
        float customColors[4][4] = {}; // Main, Accent, Text, Background
    } m_appearanceSettings;

    struct HotkeySettings {
        std::string toggleOverlay = "Escape";
        std::string captureInput = "Control+Space";
        std::string showBrowser = "Control+B";
        std::string showLinks = "Control+L";
        std::string showSettings = "Control+S";
    } m_hotkeySettings;

    // Search engine options
    std::map<std::string, std::string> m_searchEngines = {
        {"Google", "https://www.google.com/search?q=%s"},
        {"Bing", "https://www.bing.com/search?q=%s"},
        {"DuckDuckGo", "https://duckduckgo.com/?q=%s"},
        {"Yahoo", "https://search.yahoo.com/search?p=%s"}
    };

    // Resource pointer (not owned)
    UISystem* m_uiSystem = nullptr;

    // UI state
    int m_currentSection = 0;
    char m_homePageBuffer[1024] = {};
    bool m_settingsChanged = false;
};