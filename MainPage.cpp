// GameOverlay - MainPage.cpp
// Phase 3: User Interface Development
// Main page with overview and general information

#include "MainPage.h"
#include "imgui.h"
#include "GameOverlay.h"
#include <algorithm>

MainPage::MainPage() : PageBase("Main") {
    // Initialize recent items for demo
    m_recentItems = {
        { "Google", "https://www.google.com", "🔍" },
        { "YouTube", "https://www.youtube.com", "📺" },
        { "Reddit", "https://www.reddit.com", "🌐" },
        { "GitHub", "https://www.github.com", "💻" },
        { "Twitter", "https://www.twitter.com", "🐦" }
    };

    // Initialize performance history
    for (int i = 0; i < PERFORMANCE_HISTORY; i++) {
        m_fpsHistory[i] = 0;
        m_memoryHistory[i] = 0;
    }
}

void MainPage::Render() {
    ImGui::BeginChild("MainPageScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    RenderWelcomeSection();

    ImGui::Spacing();
    ImGui::Spacing();

    RenderQuickAccessSection();

    ImGui::Spacing();
    ImGui::Spacing();

    RenderPerformanceSection();

    ImGui::EndChild();

    // Update performance history
    m_fpsHistory[m_performanceHistoryIndex] = ImGui::GetIO().Framerate;
    m_memoryHistory[m_performanceHistoryIndex] = 100.0f; // Placeholder, will be replaced with actual memory usage
    m_performanceHistoryIndex = (m_performanceHistoryIndex + 1) % PERFORMANCE_HISTORY;
}

void MainPage::RenderWelcomeSection() {
    RenderSectionHeader("Welcome to GameOverlay");

    ImGui::Text("Version: %s", GAMEOVERLAY_VERSION_STRING);
    ImGui::Text("Phase: %s", GAMEOVERLAY_PHASE);
    ImGui::Spacing();

    ImGui::TextWrapped(
        "GameOverlay provides a lightweight, transparent overlay with browser "
        "capabilities for seamless access to web content during gameplay. "
        "Use the tabs above to navigate between different sections."
    );

    ImGui::Spacing();

    ImGui::BulletText("Use 'ESC' key to toggle between active and inactive states");
    ImGui::BulletText("Browse the web while gaming with full browser functionality");
    ImGui::BulletText("Save bookmarks and quick links for easy access");
    ImGui::BulletText("Customize overlay appearance and behavior in Settings");

    ImGui::Spacing();

    // Keyboard shortcut key
    if (ImGui::Button("Show Keyboard Shortcuts")) {
        ImGui::OpenPopup("KeyboardShortcutsPopup");
    }

    // Keyboard shortcuts popup
    if (ImGui::BeginPopup("KeyboardShortcutsPopup")) {
        ImGui::Text("Keyboard Shortcuts");
        ImGui::Separator();
        ImGui::Columns(2);
        ImGui::Text("ESC"); ImGui::NextColumn(); ImGui::Text("Toggle overlay active state"); ImGui::NextColumn();
        ImGui::Text("Ctrl+Tab"); ImGui::NextColumn(); ImGui::Text("Cycle through tabs"); ImGui::NextColumn();
        ImGui::Text("Alt+1-4"); ImGui::NextColumn(); ImGui::Text("Switch to specific tab"); ImGui::NextColumn();
        ImGui::Text("Alt+F4"); ImGui::NextColumn(); ImGui::Text("Exit overlay"); ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::EndPopup();
    }
}

void MainPage::RenderQuickAccessSection() {
    RenderSectionHeader("Quick Access");

    // Recent items
    ImGui::Text("Recently Visited");
    ImGui::Spacing();

    const float itemWidth = 100.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int itemsPerRow = std::max(1, static_cast<int>(windowWidth / itemWidth));

    for (int i = 0; i < m_recentItems.size(); i++) {
        const auto& item = m_recentItems[i];

        if (i % itemsPerRow != 0)
            ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Button(item.icon.c_str(), ImVec2(40, 40));
        if (ImGui::IsItemClicked()) {
            // Open URL in browser tab - would be implemented in actual integration
        }

        // Center text under icon
        float textWidth = ImGui::CalcTextSize(item.name.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (40 - textWidth) * 0.5f);
        ImGui::TextWrapped("%s", item.name.c_str());
        ImGui::EndGroup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Search bar
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1); // Full width
    if (ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Handle search action - would open browser with search query
    }
}

void MainPage::RenderPerformanceSection() {
    RenderSectionHeader("Performance Monitor");

    // FPS graph
    ImGui::Text("Framerate");
    ImGui::PlotLines("##fps", m_fpsHistory, PERFORMANCE_HISTORY, m_performanceHistoryIndex,
        nullptr, 0.0f, 120.0f, ImVec2(0, 80));
    ImGui::Text("Current: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::Spacing();

    // Memory usage graph
    ImGui::Text("Memory Usage");
    ImGui::PlotLines("##memory", m_memoryHistory, PERFORMANCE_HISTORY, m_performanceHistoryIndex,
        nullptr, 0.0f, 200.0f, ImVec2(0, 80));
    ImGui::Text("Current: %.1f MB", m_memoryHistory[m_performanceHistoryIndex == 0 ? PERFORMANCE_HISTORY - 1 : m_performanceHistoryIndex - 1]);

    ImGui::Spacing();

    // System info
    ImGui::Text("System Information");
    ImGui::BulletText("CPU: AMD Ryzen 5 5600X (Placeholder)");
    ImGui::BulletText("GPU: NVIDIA GeForce RTX 3070 (Placeholder)");
    ImGui::BulletText("RAM: 32 GB DDR4 (Placeholder)");
    ImGui::BulletText("Display: 2560x1440 @ 144Hz (Placeholder)");
}
