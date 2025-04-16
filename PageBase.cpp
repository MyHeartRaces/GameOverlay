// GameOverlay - PageBase.cpp
// Phase 3: User Interface Development
// Base class for UI pages

#include "PageBase.h"
#include "imgui.h"

void PageBase::RenderSectionHeader(const char* label) {
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Assuming font 1 is a slightly larger font
    ImGui::Text("%s", label);
    ImGui::PopFont();
    RenderSeparator();
    ImGui::Spacing();
}

void PageBase::RenderSeparator() {
    ImGui::Separator();
    ImGui::Spacing();
}

void PageBase::RenderInfoText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.5f, 1.0f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void PageBase::RenderWarningText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void PageBase::RenderErrorText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}
