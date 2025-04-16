// GameOverlay - BrowserPage.h
// Phase 3: User Interface Development
// Browser page with embedded web browser

#pragma once

#include "PageBase.h"
#include "BrowserView.h"
#include <string>
#include <vector>
#include <array>

class BrowserPage : public PageBase {
public:
    BrowserPage(BrowserView* browserView);
    ~BrowserPage() = default;

    // Render browser page content
    void Render() override;

private:
    // Browser controls section
    void RenderBrowserControls();

    // Browser view section
    void RenderBrowserView();

    // Bookmark functionality
    void SaveCurrentPageAsBookmark();
    void LoadBookmark(const std::string& url);
    void DeleteBookmark(size_t index);
    void RenderBookmarksSection();

    // Bookmarks storage
    struct Bookmark {
        std::string name;
        std::string url;
        std::string favicon;
    };

    std::vector<Bookmark> m_bookmarks;

    // URL history
    static constexpr size_t URL_HISTORY_SIZE = 10;
    std::array<std::string, URL_HISTORY_SIZE> m_urlHistory;
    size_t m_urlHistoryIndex = 0;
    size_t m_urlHistoryCount = 0;

    // Resource pointer (not owned)
    BrowserView* m_browserView = nullptr;

    // UI state
    char m_urlBuffer[1024] = {};
    char m_bookmarkNameBuffer[256] = {};
    bool m_showBookmarkDialog = false;
    bool m_isAddingBookmark = false;
};