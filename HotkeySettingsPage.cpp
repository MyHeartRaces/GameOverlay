// GameOverlay - HotkeySettingsPage.cpp
// Phase 4: Hotkey System Implementation
// Dedicated page for configuring hotkeys

#include "HotkeySettingsPage.h"
#include "imgui.h"
#include <algorithm>

HotkeySettingsPage::HotkeySettingsPage(HotkeyManager* hotkeyManager)
    : PageBase("Hotkey Settings"), m_hotkeyManager(hotkeyManager) {

    // Initialize edited hotkeys from current settings
    if (m_hotkeyManager) {
        m_editedHotkeys = m_hotkeyManager->GetHotkeys();
    }
}

void HotkeySettingsPage::Render() {
    ImGui::BeginChild("HotkeySettingsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    RenderSectionHeader("Hotkey Configuration");

    ImGui::TextWrapped(
        "Configure keyboard shortcuts for GameOverlay functions. "
        "Click on a hotkey to change it, then press the desired key combination."
    );

    ImGui::Spacing();
    ImGui::Spacing();

    // Hotkey editor
    RenderHotkeyEditor();

    // Conflict dialog (if needed)
    if (m_showConflictDialog) {
        ImGui::OpenPopup("Hotkey Conflict");
        RenderConflictDialog();
    }

    ImGui::Spacing();

    // Apply button
    if (ImGui::Button("Apply Changes", ImVec2(150, 0))) {
        ApplyChanges();
    }

    ImGui::SameLine();

    // Reset all hotkeys button
    if (ImGui::Button("Reset All", ImVec2(100, 0))) {
        ImGui::OpenPopup("Reset Confirmation");
    }

    // Reset confirmation popup
    if (ImGui::BeginPopupModal("Reset Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all hotkeys to default values?");
        ImGui::Text("This action cannot be undone.");
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            // Reset to defaults (reinitialize hotkey manager)
            if (m_hotkeyManager) {
                // Clear existing hotkeys
                for (const auto& [action, hotkey] : m_editedHotkeys) {
                    m_hotkeyManager->UnregisterHotkey(action);
                }

                // Register default hotkeys again
                m_hotkeyManager->RegisterDefaultHotkeys();

                // Refresh edited hotkeys
                m_editedHotkeys = m_hotkeyManager->GetHotkeys();
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void HotkeySettingsPage::RenderHotkeyEditor() {
    // Hotkey table
    if (ImGui::BeginTable("HotkeyTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Hotkey", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        // Sort actions alphabetically by description
        struct ActionEntry {
            std::string id;
            std::string description;
            Hotkey hotkey;
        };

        std::vector<ActionEntry> sortedActions;
        for (const auto& [action, hotkey] : m_editedHotkeys) {
            std::string description = action;
            auto it = m_actionDescriptions.find(action);
            if (it != m_actionDescriptions.end()) {
                description = it->second;
            }

            sortedActions.push_back({ action, description, hotkey });
        }

        std::sort(sortedActions.begin(), sortedActions.end(),
            [](const ActionEntry& a, const ActionEntry& b) {
                return a.description < b.description;
            });

        // Render rows
        for (const auto& entry : sortedActions) {
            ImGui::TableNextRow();

            // Action description
            ImGui::TableNextColumn();
            ImGui::Text("%s", entry.description.c_str());

            // Hotkey field
            ImGui::TableNextColumn();
            ImGui::PushID(entry.id.c_str());

            std::string hotkeyText = entry.hotkey.ToString();
            bool isEditing = (m_capturingHotkey && m_currentEditAction == entry.id);

            ImGui::PushStyleColor(ImGuiCol_Button,
                isEditing ? ImVec4(0.3f, 0.6f, 0.3f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

            if (ImGui::Button(isEditing ? "Press any key..." : hotkeyText.c_str(), ImVec2(-1, 0))) {
                // Start capturing
                if (!m_capturingHotkey) {
                    m_capturingHotkey = true;
                    m_currentEditAction = entry.id;
                    m_pendingHotkey = Hotkey(); // Clear pending hotkey
                }
            }

            ImGui::PopStyleColor();

            // Capture keys while in edit mode
            if (isEditing) {
                // Capture key presses
                bool keyPressed = false;

                // Check modifier keys
                bool ctrl = ImGui::GetIO().KeyCtrl;
                bool alt = ImGui::GetIO().KeyAlt;
                bool shift = ImGui::GetIO().KeyShift;

                // Check for any key press
                for (int i = 0x08; i <= 0xFE; i++) { // Starting from VK_BACK (8) to avoid control keys
                    if (ImGui::IsKeyPressed(i)) {
                        // Create new hotkey
                        m_pendingHotkey = Hotkey(i, ctrl, alt, shift, false);
                        keyPressed = true;
                        break;
                    }
                }

                // If a key was pressed, check for conflicts
                if (keyPressed) {
                    m_capturingHotkey = false;

                    // Check if this conflicts with another hotkey
                    if (CheckForConflicts(m_currentEditAction, m_pendingHotkey)) {
                        m_showConflictDialog = true;
                    }
                    else {
                        // No conflict, update hotkey
                        m_editedHotkeys[m_currentEditAction] = m_pendingHotkey;
                    }
                }

                // Allow escape to cancel
                if (ImGui::IsKeyPressed(VK_ESCAPE)) {
                    m_capturingHotkey = false;
                }
            }

            // Options column
            ImGui::TableNextColumn();

            if (ImGui::Button("Clear")) {
                m_editedHotkeys[entry.id] = Hotkey(); // Empty hotkey
            }

            ImGui::SameLine();

            if (ImGui::Button("Reset")) {
                // Reset to default (would normally load from defaults)
                if (m_hotkeyManager) {
                    // Get the current registered hotkey
                    auto registeredHotkeys = m_hotkeyManager->GetHotkeys();
                    auto it = registeredHotkeys.find(entry.id);
                    if (it != registeredHotkeys.end()) {
                        m_editedHotkeys[entry.id] = it->second;
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void HotkeySettingsPage::RenderConflictDialog() {
    // Centered popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Hotkey Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Get description of conflicting action
        std::string conflictDescription = m_conflictingAction;
        auto it = m_actionDescriptions.find(m_conflictingAction);
        if (it != m_actionDescriptions.end()) {
            conflictDescription = it->second;
        }

        // Get description of current action
        std::string currentDescription = m_currentEditAction;
        it = m_actionDescriptions.find(m_currentEditAction);
        if (it != m_actionDescriptions.end()) {
            currentDescription = it->second;
        }

        ImGui::Text("The hotkey %s is already assigned to:", m_pendingHotkey.ToString().c_str());
        ImGui::Text("\"%s\"", conflictDescription.c_str());
        ImGui::Spacing();
        ImGui::Text("Do you want to reassign it to:");
        ImGui::Text("\"%s\"?", currentDescription.c_str());

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Yes, Reassign", ImVec2(120, 0))) {
            // Remove from conflicting action
            m_editedHotkeys[m_conflictingAction] = Hotkey();

            // Assign to current action
            m_editedHotkeys[m_currentEditAction] = m_pendingHotkey;

            m_showConflictDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No, Cancel", ImVec2(120, 0))) {
            m_showConflictDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool HotkeySettingsPage::CheckForConflicts(const std::string& actionName, const Hotkey& hotkey) {
    if (hotkey.IsEmpty()) return false;

    for (const auto& [action, existing] : m_editedHotkeys) {
        if (action != actionName && existing == hotkey) {
            m_conflictingAction = action;
            return true;
        }
    }

    return false;
}

void HotkeySettingsPage::ApplyChanges() {
    if (!m_hotkeyManager) return;

    // Get current hotkeys
    auto currentHotkeys = m_hotkeyManager->GetHotkeys();

    // Update hotkeys in the manager
    for (const auto& [action, hotkey] : m_editedHotkeys) {
        // Check if this hotkey has changed
        auto it = currentHotkeys.find(action);
        if (it == currentHotkeys.end() || !(it->second == hotkey)) {
            // Update the hotkey
            m_hotkeyManager->UpdateHotkey(action, hotkey);
        }
    }
}
