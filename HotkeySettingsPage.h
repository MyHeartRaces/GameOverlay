// GameOverlay - HotkeySettingsPage.h
// Phase 4: Hotkey System Implementation
// Dedicated page for configuring hotkeys

#pragma once

#include "PageBase.h"
#include "HotkeyManager.h"
#include <string>
#include <map>

class HotkeySettingsPage : public PageBase {
public:
    HotkeySettingsPage(HotkeyManager* hotkeyManager);
    ~HotkeySettingsPage() = default;

    // Render hotkey settings page content
    void Render() override;

private:
    // Render hotkey editing section
    void RenderHotkeyEditor();

    // Render conflict resolution dialog
    void RenderConflictDialog();

    // Apply hotkey changes
    void ApplyChanges();

    // Check for conflicts with existing hotkeys
    bool CheckForConflicts(const std::string& actionName, const Hotkey& hotkey);

    // Resource pointer (not owned)
    HotkeyManager* m_hotkeyManager = nullptr;

    // UI state
    bool m_capturingHotkey = false;
    std::string m_currentEditAction;
    Hotkey m_pendingHotkey;
    std::string m_conflictingAction;
    bool m_showConflictDialog = false;

    // Temporary copy of hotkeys for editing
    std::map<std::string, Hotkey> m_editedHotkeys;

    // Human-readable action names
    std::map<std::string, std::string> m_actionDescriptions = {
        {"toggle_overlay", "Toggle Overlay Active/Inactive"},
        {"show_hide", "Show/Hide Overlay Window"},
        {"show_main", "Switch to Main Page"},
        {"show_browser", "Switch to Browser Page"},
        {"show_links", "Switch to Links Page"},
        {"show_settings", "Switch to Settings Page"},
        {"browser_back", "Browser Back"},
        {"browser_forward", "Browser Forward"},
        {"browser_refresh", "Browser Refresh"},
        {"browser_stop", "Browser Stop Loading"},
        {"browser_home", "Browser Home"}
    };
};