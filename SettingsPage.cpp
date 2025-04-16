// GameOverlay - SettingsPage.cpp
// Phase 3: User Interface Development
// Settings page for application configuration

#include "SettingsPage.h"
#include "UISystem.h"
#include "GameOverlay.h"
#include "imgui.h"
#include <cstring>

SettingsPage::SettingsPage(UISystem* uiSystem)
    : PageBase("Settings"), m_uiSystem(uiSystem) {

    // Set home page buffer from default settings
    strcpy_s(m_homePageBuffer, m_browserSettings.homePage.c_str());

    // Initialize custom colors with defaults
    m_appearanceSettings.customColors[0][0] = 0.2f; // Main R
    m_appearanceSettings.customColors[0][1] = 0.2f; // Main G
    m_appearanceSettings.customColors[0][2] = 0.6f; // Main B
    m_appearanceSettings.customColors[0][3] = 1.0f; // Main A

    m_appearanceSettings.customColors[1][0] = 0.3f; // Accent R
    m_appearanceSettings.customColors[1][1] = 0.3f; // Accent G
    m_appearanceSettings.customColors[1][2] = 0.7f; // Accent B
    m_appearanceSettings.customColors[1][3] = 1.0f; // Accent A

    m_appearanceSettings.customColors[2][0] = 0.9f; // Text R
    m_appearanceSettings.customColors[2][1] = 0.9f; // Text G
    m_appearanceSettings.customColors[2][2] = 0.9f; // Text B
    m_appearanceSettings.customColors[2][3] = 1.0f; // Text A

    m_appearanceSettings.customColors[3][0] = 0.1f; // Background R
    m_appearanceSettings.customColors[3][1] = 0.1f; // Background G
    m_appearanceSettings.customColors[3][2] = 0.15f; // Background B
    m_appearanceSettings.customColors[3][3] = 0.95f; // Background A
}

void SettingsPage::Render() {
    // Settings panel with left sidebar and right content area
    ImGui::Columns(2, "SettingsColumns", true);
    ImGui::SetColumnWidth(0, 180.0f);

    // Left sidebar for settings categories
    const char* sections[] = {
        "General", "Browser", "Appearance", "Hotkeys", "About"
    };

    ImGui::BeginChild("SettingsSidebar");

    for (int i = 0; i < IM_ARRAYSIZE(sections); i++) {
        bool isSelected = (m_currentSection == i);
        if (ImGui::Selectable(sections[i], isSelected)) {
            m_currentSection = i;
        }
    }

    ImGui::EndChild();

    // Right content area
    ImGui::NextColumn();

    ImGui::BeginChild("SettingsContent", ImVec2(0, -30)); // Leave space for buttons at bottom

    // Render the selected settings section
    switch (m_currentSection) {
    case 0: RenderGeneralSettings(); break;
    case 1: RenderBrowserSettings(); break;
    case 2: RenderAppearanceSettings(); break;
    case 3: RenderHotkeySettings(); break;
    case 4: RenderAboutSection(); break;
    }

    ImGui::EndChild();

    // Bottom buttons
    if (m_currentSection < 4) { // Don't show for "About" section
        if (ImGui::Button("Apply Changes", ImVec2(120, 0))) {
            // Apply settings for the current section
            switch (m_currentSection) {
            case 0: ApplyGeneralSettings(); break;
            case 1: ApplyBrowserSettings(); break;
            case 2: ApplyAppearanceSettings(); break;
            case 3: ApplyHotkeySettings(); break;
            }

            m_settingsChanged = false;
        }

        ImGui::SameLine();

        ImGui::BeginDisabled(!m_settingsChanged);
        if (ImGui::Button("Reset", ImVec2(80, 0))) {
            // Reset settings in the current section to defaults
            switch (m_currentSection) {
            case 0:
                m_generalSettings = GeneralSettings();
                break;
            case 1:
                m_browserSettings = BrowserSettings();
                strcpy_s(m_homePageBuffer, m_browserSettings.homePage.c_str());
                break;
            case 2:
                m_appearanceSettings = AppearanceSettings();
                break;
            case 3:
                m_hotkeySettings = HotkeySettings();
                break;
            }
            m_settingsChanged = false;
        }
        ImGui::EndDisabled();
    }

    ImGui::Columns(1);
}

void SettingsPage::RenderGeneralSettings() {
    RenderSectionHeader("General Settings");

    bool changed = false;

    changed |= ImGui::Checkbox("Start with Windows", &m_generalSettings.startWithWindows);
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Launch GameOverlay automatically when Windows starts");
    }

    changed |= ImGui::Checkbox("Start Minimized", &m_generalSettings.startMinimized);
    changed |= ImGui::Checkbox("Check for Updates", &m_generalSettings.checkForUpdates);
    changed |= ImGui::Checkbox("Auto-hide when Inactive", &m_generalSettings.autoHide);

    if (m_generalSettings.autoHide) {
        ImGui::Indent();
        changed |= ImGui::SliderInt("Auto-hide Delay (seconds)", &m_generalSettings.autoHideDelay, 1, 30);
        ImGui::Unindent();
    }

    ImGui::Spacing();

    ImGui::Text("Inactive Overlay Opacity");
    changed |= ImGui::SliderInt("##InactiveOpacity", &m_generalSettings.inactiveOpacity, 0, 100, "%d%%");

    if (changed) {
        m_settingsChanged = true;
    }
}

void SettingsPage::RenderBrowserSettings() {
    RenderSectionHeader("Browser Settings");

    bool changed = false;

    changed |= ImGui::Checkbox("Enable JavaScript", &m_browserSettings.enableJavaScript);
    changed |= ImGui::Checkbox("Enable Browser Plugins", &m_browserSettings.enablePlugins);
    changed |= ImGui::Checkbox("Enable Cookies", &m_browserSettings.enableCookies);
    changed |= ImGui::Checkbox("Clear Cache on Exit", &m_browserSettings.clearCacheOnExit);
    changed |= ImGui::Checkbox("Clear History on Exit", &m_browserSettings.clearHistoryOnExit);

    ImGui::Spacing();

    ImGui::Text("Home Page");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##HomePage", m_homePageBuffer, sizeof(m_homePageBuffer))) {
        changed = true;
    }

    ImGui::Spacing();

    ImGui::Text("Default Search Engine");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##SearchEngine", m_browserSettings.searchEngine.c_str())) {
        for (const auto& engine : m_searchEngines) {
            bool isSelected = (m_browserSettings.searchEngine == engine.first);
            if (ImGui::Selectable(engine.first.c_str(), isSelected)) {
                m_browserSettings.searchEngine = engine.first;
                changed = true;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (changed) {
        m_settingsChanged = true;
    }
}

void SettingsPage::RenderAppearanceSettings() {
    RenderSectionHeader("Appearance Settings");

    bool changed = false;

    // Theme selection
    ImGui::Text("UI Theme");
    const char* themes[] = { "Dark", "Light", "Classic" };
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("##Theme", &m_appearanceSettings.theme, themes, IM_ARRAYSIZE(themes))) {
        changed = true;
    }

    ImGui::Spacing();

    // Font size
    ImGui::Text("Font Size");
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##FontSize", &m_appearanceSettings.fontSize, 0.7f, 1.5f, "%.1f")) {
        changed = true;
    }

    ImGui::Spacing();

    // Window dimensions
    ImGui::Text("Default Window Size");
    ImGui::SetNextItemWidth(200);
    changed |= ImGui::InputInt("Width##WindowWidth", &m_appearanceSettings.windowWidth, 10);
    ImGui::SetNextItemWidth(200);
    changed |= ImGui::InputInt("Height##WindowHeight", &m_appearanceSettings.windowHeight, 10);

    // Ensure valid window dimensions
    m_appearanceSettings.windowWidth = std::max(640, m_appearanceSettings.windowWidth);
    m_appearanceSettings.windowHeight = std::max(480, m_appearanceSettings.windowHeight);

    ImGui::Spacing();
    ImGui::Separator();

    // Custom colors
    if (ImGui::Checkbox("Use Custom Colors", &m_appearanceSettings.useCustomColors)) {
        changed = true;
    }

    if (m_appearanceSettings.useCustomColors) {
        ImGui::Spacing();

        const char* colorNames[] = { "Main", "Accent", "Text", "Background" };
        for (int i = 0; i < 4; i++) {
            ImGui::Text("%s Color", colorNames[i]);
            if (ImGui::ColorEdit4(("##" + std::string(colorNames[i])).c_str(), m_appearanceSettings.customColors[i])) {
                changed = true;
            }
            ImGui::Spacing();
        }
    }

    if (changed) {
        m_settingsChanged = true;
    }
}

void SettingsPage::RenderHotkeySettings() {
    RenderSectionHeader("Hotkey Settings");

    bool changed = false;

    // Helper lambda for hotkey editing
    auto RenderHotkey = [&changed](const char* label, std::string& hotkeyValue) {
        ImGui::Text("%s", label);

        ImGui::SetNextItemWidth(200);
        char buffer[64] = {};
        strcpy_s(buffer, hotkeyValue.c_str());

        ImGui::PushID(label);
        if (ImGui::InputText("##Hotkey", buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly)) {
            // This should never happen since the field is read-only,
            // but we include it for completeness
            hotkeyValue = buffer;
            changed = true;
        }

        // Show as active when clicked
        static bool isEditing = false;
        static const char* currentEditLabel = nullptr;

        if (ImGui::IsItemClicked()) {
            isEditing = true;
            currentEditLabel = label;
        }

        if (isEditing && currentEditLabel == label) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Press any key...");

            // In a real implementation, this would capture the next keypress
            // For now, just finish editing on any key press
            for (int i = 0; i < 512; i++) {
                if (ImGui::IsKeyPressed(i)) {
                    isEditing = false;
                    // This would be replaced with actual key name conversion
                    hotkeyValue = "Key" + std::to_string(i);
                    changed = true;
                    break;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset##Reset")) {
            isEditing = false;
            switch (label[0]) {
            case 'T': hotkeyValue = "Escape"; break;          // Toggle Overlay
            case 'C': hotkeyValue = "Control+Space"; break;   // Capture Input
            case 'S' && label[1] == 'h': hotkeyValue = "Control+B"; break; // Show Browser
            case 'S' && label[1] == 'h' && label[5] == 'L': hotkeyValue = "Control+L"; break; // Show Links
            case 'S' && label[1] == 'h' && label[5] == 'S': hotkeyValue = "Control+S"; break; // Show Settings
            }
            changed = true;
        }
        ImGui::PopID();

        ImGui::Spacing();
        };

    RenderHotkey("Toggle Overlay", m_hotkeySettings.toggleOverlay);
    RenderHotkey("Capture Input", m_hotkeySettings.captureInput);
    RenderHotkey("Show Browser", m_hotkeySettings.showBrowser);
    RenderHotkey("Show Links", m_hotkeySettings.showLinks);
    RenderHotkey("Show Settings", m_hotkeySettings.showSettings);

    if (changed) {
        m_settingsChanged = true;
    }
}

void SettingsPage::RenderAboutSection() {
    RenderSectionHeader("About GameOverlay");

    ImGui::Text("GameOverlay - Version %s", GAMEOVERLAY_VERSION_STRING);
    ImGui::Text("Development Phase: %s", GAMEOVERLAY_PHASE);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextWrapped(
        "GameOverlay is a lightweight, transparent overlay application that provides "
        "browser functionality without injecting code into games. It allows you to "
        "browse the web, access guides, chat, and other online content while gaming "
        "with minimal impact on performance."
    );

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Technology Stack:");
    ImGui::BulletText("C++20: Modern C++ features for clean, efficient code");
    ImGui::BulletText("DirectX 11: Hardware-accelerated rendering for the overlay");
    ImGui::BulletText("Chromium Embedded Framework (CEF): Self-contained browser engine");
    ImGui::BulletText("Dear ImGui: Immediate-mode GUI for efficient UI rendering");
    ImGui::BulletText("Windows API: Window management for transparent overlay");

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("System Information")) {
        ImGui::Text("CPU: AMD Ryzen 5 5600X (Placeholder)");
        ImGui::Text("GPU: NVIDIA GeForce RTX 3070 (Placeholder)");
        ImGui::Text("RAM: 32 GB DDR4 (Placeholder)");
        ImGui::Text("OS: Windows 10 64-bit (Placeholder)");
        ImGui::Text("DirectX Version: 11.0 (Placeholder)");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Check for Updates")) {
        // Would connect to update server in a real implementation
        ImGui::OpenPopup("UpdatePopup");
    }

    ImGui::SameLine();

    if (ImGui::Button("View License")) {
        ImGui::OpenPopup("LicensePopup");
    }

    // Update popup
    if (ImGui::BeginPopupModal("UpdatePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Checking for updates...");
        ImGui::Separator();

        static float progress = 0.0f;
        progress += 0.01f;
        if (progress > 1.0f) progress = 0.0f;

        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

        if (progress > 0.9f) {
            ImGui::Text("You are running the latest version!");
        }

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // License popup
    if (ImGui::BeginPopupModal("LicensePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("GameOverlay License Agreement");
        ImGui::Separator();

        ImGui::BeginChild("LicenseText", ImVec2(500, 300), true);
        ImGui::TextWrapped(
            "MIT License\n\n"
            "Copyright (c) 2025 GameOverlay Developers\n\n"
            "Permission is hereby granted, free of charge, to any person obtaining a copy "
            "of this software and associated documentation files (the \"Software\"), to deal "
            "in the Software without restriction, including without limitation the rights "
            "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
            "copies of the Software, and to permit persons to whom the Software is "
            "furnished to do so, subject to the following conditions:\n\n"
            "The above copyright notice and this permission notice shall be included in all "
            "copies or substantial portions of the Software.\n\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
            "SOFTWARE."
        );
        ImGui::EndChild();

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SettingsPage::ApplyGeneralSettings() {
    // In a real implementation, this would save settings to config file
    // and apply changes to the application

    // For now, just mark as applied
    m_settingsChanged = false;
}

void SettingsPage::ApplyBrowserSettings() {
    // Update home page from buffer
    m_browserSettings.homePage = m_homePageBuffer;

    // In a real implementation, this would apply changes to the browser

    // For now, just mark as applied
    m_settingsChanged = false;
}

void SettingsPage::ApplyAppearanceSettings() {
    // Apply theme change
    if (m_uiSystem) {
        UISystem::Theme theme;
        switch (m_appearanceSettings.theme) {
        case 0: theme = UISystem::Theme::Dark; break;
        case 1: theme = UISystem::Theme::Light; break;
        case 2: theme = UISystem::Theme::Classic; break;
        default: theme = UISystem::Theme::Dark; break;
        }

        m_uiSystem->SetTheme(theme);
    }

    // In a real implementation, this would apply font size and custom colors

    // For now, just mark as applied
    m_settingsChanged = false;
}

void SettingsPage::ApplyHotkeySettings() {
    // In a real implementation, this would register the new hotkeys

    // For now, just mark as applied
    m_settingsChanged = false;
}
