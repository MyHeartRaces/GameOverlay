// GameOverlay - MainPage.h
// Phase 3: User Interface Development
// Main page with overview and general information

#pragma once

#include "PageBase.h"
#include <string>
#include <vector>

class MainPage : public PageBase {
public:
    MainPage();
    ~MainPage() = default;

    // Render main page content
    void Render() override;

private:
    // Render welcome section
    void RenderWelcomeSection();

    // Render quick access section
    void RenderQuickAccessSection();

    // Render performance section
    void RenderPerformanceSection();

    // Recent items
    struct RecentItem {
        std::string name;
        std::string url;
        std::string icon;
    };

    std::vector<RecentItem> m_recentItems;

    // Performance data for graph
    static constexpr int PERFORMANCE_HISTORY = 90;
    float m_fpsHistory[PERFORMANCE_HISTORY] = {};
    float m_memoryHistory[PERFORMANCE_HISTORY] = {};
    int m_performanceHistoryIndex = 0;
};