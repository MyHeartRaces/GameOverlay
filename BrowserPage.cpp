// GameOverlay - BrowserPage.cpp
// Phase 3: User Interface Development
// Browser page with embedded web browser

#include "BrowserPage.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>

BrowserPage::BrowserPage(BrowserView* browserView)
    : PageBase("Browser"), m_browserView(browserView) {

    // Initialize URL buffer
    strcpy_s(m_urlBuffer, "https://www.google.com");

    // Initialize bookmark examples for demo
    m_bookmarks = {
        { "Google", "https://www.google.com", "🔍" },
        { "YouTube", "https://www.youtube.com", "📺" },
        { "Reddit", "https://www.reddit.com", "🌐" },
        { "GitHub", "https://www.github.com", "💻" },
        { "Wikipedia", "https://www.wikipedia.org", "📚" }
    };

    // Clear URL history
    for (auto& url : m_urlHistory) {
        url.clear();
    }
}

void BrowserPage::Render() {
    // Render browser UI components
    RenderBrowserControls();

    ImGui::Spacing();

    // Render browser view
    RenderBrowserView();

    // Render bookmarks section at the bottom
    RenderBookmarksSection();

    // Bookmark dialog
    if (m_showBookmarkDialog) {
        ImGui::OpenPopup("Bookmark Dialog");
        m_showBookmarkDialog = false;
    }

    // Centered popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Bookmark Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_isAddingBookmark) {
            ImGui::Text("Add Bookmark");
            ImGui::Separator();

            // Pre-fill with current page title and URL
            if (m_browserView && m_browserView->GetBrowserManager()) {
                std::string currentTitle = m_browserView->GetBrowserManager()->GetTitle();
                std::string currentUrl = m_browserView->GetBrowserManager()->GetURL();

                if (strlen(m_bookmarkNameBuffer) == 0 && !currentTitle.empty()) {
                    strcpy_s(m_bookmarkNameBuffer, currentTitle.c_str());
                }

                ImGui::Text("URL: %s", currentUrl.c_str());
            }

            ImGui::InputText("Name", m_bookmarkNameBuffer, sizeof(m_bookmarkNameBuffer));

            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                SaveCurrentPageAsBookmark();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        else {
            ImGui::Text("Bookmark Management");
            ImGui::Separator();

            if (ImGui::BeginTable("BookmarksTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < m_bookmarks.size(); i++) {
                    ImGui::TableNextRow();

                    // Name column
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_bookmarks[i].name.c_str());

                    // URL column
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_bookmarks[i].url.c_str());

                    // Action column
                    ImGui::TableNextColumn();
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Button("Delete")) {
                        DeleteBookmark(i);
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();

            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void BrowserPage::RenderBrowserControls() {
    // Navigation buttons
    if (ImGui::Button("Back") && m_browserView && m_browserView->GetBrowserManager() &&
        m_browserView->GetBrowserManager()->CanGoBack()) {
        m_browserView->GetBrowserManager()->GoBack();
    }

    ImGui::SameLine();

    if (ImGui::Button("Forward") && m_browserView && m_browserView->GetBrowserManager() &&
        m_browserView->GetBrowserManager()->CanGoForward()) {
        m_browserView->GetBrowserManager()->GoForward();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload") && m_browserView && m_browserView->GetBrowserManager()) {
        m_browserView->GetBrowserManager()->Reload();
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop") && m_browserView && m_browserView->GetBrowserManager() &&
        m_browserView->GetBrowserManager()->IsLoading()) {
        m_browserView->GetBrowserManager()->StopLoad();
    }

    ImGui::SameLine();

    if (ImGui::Button("Home")) {
        if (m_browserView) {
            m_browserView->Navigate("https://www.google.com");
        }
    }

    ImGui::SameLine();

    // Add bookmark button
    if (ImGui::Button("Add Bookmark")) {
        m_isAddingBookmark = true;
        m_bookmarkNameBuffer[0] = '\0'; // Clear buffer
        m_showBookmarkDialog = true;
    }

    ImGui::SameLine();

    // Manage bookmarks button
    if (ImGui::Button("Manage Bookmarks")) {
        m_isAddingBookmark = false;
        m_showBookmarkDialog = true;
    }

    // URL input bar
    ImGui::SetNextItemWidth(-1); // Full width
    if (ImGui::InputText("URL", m_urlBuffer, sizeof(m_urlBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Check if input has http/https, add if missing
        std::string url = m_urlBuffer;
        if (url.find("://") == std::string::npos) {
            url = "https://" + url;
            strcpy_s(m_urlBuffer, url.c_str());
        }

        // Navigate to URL
        if (m_browserView) {
            m_browserView->Navigate(url);

            // Add to history
            m_urlHistory[m_urlHistoryIndex] = url;
            m_urlHistoryIndex = (m_urlHistoryIndex + 1) % URL_HISTORY_SIZE;
            m_urlHistoryCount = std::min(m_urlHistoryCount + 1, URL_HISTORY_SIZE);
        }
    }

    // Status info
    if (m_browserView && m_browserView->GetBrowserManager()) {
        std::string currentUrl = m_browserView->GetBrowserManager()->GetURL();
        std::string currentTitle = m_browserView->GetBrowserManager()->GetTitle();
        bool isLoading = m_browserView->GetBrowserManager()->IsLoading();

        // Update URL buffer if changed externally (navigation, etc.)
        if (!currentUrl.empty() && currentUrl != m_urlBuffer) {
            strcpy_s(m_urlBuffer, currentUrl.c_str());
        }

        // Show loading status or title
        ImGui::Text("%s %s",
            isLoading ? "Loading:" : "Title:",
            isLoading ? currentUrl.c_str() : currentTitle.c_str());
    }
}

void BrowserPage::RenderBrowserView() {
    // Calculate available space for browser view
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    availSize.y -= 100; // Reserve space for bookmarks bar at bottom

    // Create a child window for the browser view
    ImGui::BeginChild("BrowserView", availSize, true, ImGuiWindowFlags_NoScrollbar);

    // Render browser content if available
    if (m_browserView && m_browserView->GetShaderResourceView()) {
        ImGui::Image(
            (void*)m_browserView->GetShaderResourceView(),
            ImGui::GetContentRegionAvail()
        );
    }
    else {
        // Show placeholder if browser not available
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Browser content not available.");
    }

    ImGui::EndChild();
}

void BrowserPage::RenderBookmarksSection() {
    ImGui::BeginChild("BookmarksBar", ImVec2(0, 80), true);

    ImGui::Text("Bookmarks");
    ImGui::Separator();

    // Render bookmarks as horizontal buttons
    float buttonWidth = 100.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int buttonsPerRow = std::max(1, static_cast<int>(windowWidth / buttonWidth));

    for (int i = 0; i < m_bookmarks.size(); i++) {
        if (i % buttonsPerRow != 0)
            ImGui::SameLine();

        // Create button with bookmark name
        if (ImGui::Button(m_bookmarks[i].name.c_str(), ImVec2(buttonWidth - 10, 0))) {
            // Navigate to bookmark URL
            LoadBookmark(m_bookmarks[i].url);
        }

        // Tooltip with URL
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", m_bookmarks[i].url.c_str());
        }
    }

    ImGui::EndChild();
}

void BrowserPage::SaveCurrentPageAsBookmark() {
    if (!m_browserView || !m_browserView->GetBrowserManager())
        return;

    // Get name and URL
    std::string name = m_bookmarkNameBuffer;
    std::string url = m_browserView->GetBrowserManager()->GetURL();

    // Trim name if needed
    if (name.empty()) {
        name = m_browserView->GetBrowserManager()->GetTitle();
        if (name.empty()) {
            name = "Unnamed Bookmark";
        }
    }

    // Don't add if URL is empty
    if (url.empty())
        return;

    // Check if bookmark already exists
    auto it = std::find_if(m_bookmarks.begin(), m_bookmarks.end(),
        [&url](const Bookmark& bm) { return bm.url == url; });

    // If exists, update name, otherwise add new
    if (it != m_bookmarks.end()) {
        it->name = name;
    }
    else {
        m_bookmarks.push_back({ name, url, "🔖" });
    }

    // Clear bookmark name buffer
    m_bookmarkNameBuffer[0] = '\0';
}

void BrowserPage::LoadBookmark(const std::string& url) {
    if (m_browserView) {
        m_browserView->Navigate(url);
        strcpy_s(m_urlBuffer, url.c_str());
    }
}

void BrowserPage::DeleteBookmark(size_t index) {
    if (index < m_bookmarks.size()) {
        m_bookmarks.erase(m_bookmarks.begin() + index);
    }
}