// GameOverlay - BrowserPage.cpp
// Browser page with embedded web browser

#include "BrowserPage.h"
#include "imgui.h"
#include <algorithm>
#include <cctype> // For std::min/max if needed, <algorithm> includes it
#include <string> // For string operations

BrowserPage::BrowserPage(BrowserView* browserView)
    : PageBase("Browser"), m_browserView(browserView) {

    // Initialize URL buffer with a default page
    if (m_browserView && m_browserView->GetBrowserManager()) {
         // Try to get initial URL if browser was created with one, else default
         std::string initialUrl = m_browserView->GetBrowserManager()->GetURL();
         if (initialUrl.empty() || initialUrl == "about:blank") {
              strcpy_s(m_urlBuffer, "https://www.google.com");
         } else {
              strncpy_s(m_urlBuffer, initialUrl.c_str(), sizeof(m_urlBuffer) - 1);
         }
    } else {
        strcpy_s(m_urlBuffer, "https://www.google.com");
    }


    // Initialize bookmark examples for demo
    m_bookmarks = {
        { "Google", "https://www.google.com", "🔍" },
        { "YouTube", "https://www.youtube.com", "📺" },
        { "Reddit", "https://www.reddit.com", "🌐" },
        { "GitHub", "https://www.github.com", "💻" },
        { "Wikipedia", "https://www.wikipedia.org", "📚" }
    };

    // Clear URL history (arrays initialize members to default, which is empty string)
    // No explicit loop needed unless elements aren't default constructible.
}

void BrowserPage::Render() {
    // Render browser UI components (address bar, buttons)
    RenderBrowserControls();

    ImGui::Spacing();
    ImGui::Separator(); // Add separator
    ImGui::Spacing();

    // Render browser view itself
    RenderBrowserView();

    ImGui::Spacing();
    ImGui::Separator(); // Add separator
    ImGui::Spacing();

    // Render bookmarks section at the bottom
    RenderBookmarksSection();

    // Handle bookmark dialog popup logic
    if (m_showBookmarkDialog) {
        // OpenPopup must be called before BeginPopupModal
        ImGui::OpenPopup("Bookmark Dialog");
        m_showBookmarkDialog = false; // Reset flag after opening
    }
    RenderBookmarkDialog(); // Render the dialog content if open
}

void BrowserPage::RenderBrowserControls() {
    BrowserManager* mgr = m_browserView ? m_browserView->GetBrowserManager() : nullptr;

    // Navigation buttons
    if (ImGui::Button("Back") && mgr && mgr->CanGoBack()) {
        mgr->GoBack();
    }
    ImGui::SameLine();
    if (ImGui::Button("Forward") && mgr && mgr->CanGoForward()) {
        mgr->GoForward();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload") && mgr) {
        mgr->Reload(); // Use simple reload by default
        // Use mgr->Reload(true) for ignore cache version
    }
    ImGui::SameLine();
    bool isLoading = mgr && mgr->IsLoading();
    if (ImGui::Button("Stop") && isLoading) {
        mgr->StopLoad();
    }
    // Optionally disable Stop button when not loading
    // ImGui::BeginDisabled(!isLoading); ImGui::Button("Stop"); ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Home") && m_browserView) {
        // TODO: Make home page configurable via settings
        m_browserView->Navigate("https://www.google.com");
        strcpy_s(m_urlBuffer, "https://www.google.com");
    }

    ImGui::Spacing(); // Add space before bookmark buttons

    // Add bookmark button
    if (ImGui::Button("Add Bookmark")) {
        m_isAddingBookmark = true;
        m_bookmarkNameBuffer[0] = '\0'; // Clear buffer
        m_showBookmarkDialog = true;    // Set flag to open dialog in Render()
    }
    ImGui::SameLine();
    // Manage bookmarks button
    if (ImGui::Button("Manage Bookmarks")) {
        m_isAddingBookmark = false;
        m_showBookmarkDialog = true;    // Set flag to open dialog in Render()
    }

    ImGui::Spacing();

    // URL input bar
    ImGui::PushItemWidth(-1); // Full width available
    if (ImGui::InputText("##URLInput", m_urlBuffer, sizeof(m_urlBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string url = m_urlBuffer;
        // Add https:// prefix if no protocol is present
        if (!url.empty() && url.find("://") == std::string::npos && url.find("about:") != 0 && url.find("data:") != 0) {
            // Check for common TLDs or presence of '.' to guess if it's a URL vs search term
            if (url.find('.') != std::string::npos) {
                 url = "https://" + url;
            } else {
                 // Treat as search query (e.g., Google search)
                 // TODO: Use search engine from settings
                 url = "https://www.google.com/search?q=" + url;
            }
            // Update buffer only if modified
             strncpy_s(m_urlBuffer, url.c_str(), sizeof(m_urlBuffer) - 1);
        }

        // Navigate if view exists
        if (m_browserView) {
            m_browserView->Navigate(url);
            // Add to history only if navigation likely started (URL isn't blank)
            if (!url.empty()) {
                // Avoid adding duplicates consecutively
                 if (m_urlHistoryCount == 0 || m_urlHistory[(m_urlHistoryIndex + URL_HISTORY_SIZE - 1) % URL_HISTORY_SIZE] != url) {
                    m_urlHistory[m_urlHistoryIndex] = url;
                    m_urlHistoryIndex = (m_urlHistoryIndex + 1) % URL_HISTORY_SIZE;
                    m_urlHistoryCount = std::min(m_urlHistoryCount + 1, URL_HISTORY_SIZE);
                }
            }
        }
    }
    ImGui::PopItemWidth();

    // Status info (Loading or Title)
    if (mgr) {
        std::string currentUrl = mgr->GetURL();
        // Update URL buffer if changed externally (e.g., link click)
        // Only update if the input field is not focused to avoid interrupting typing
        if (!ImGui::IsItemActive() && !currentUrl.empty() && currentUrl != m_urlBuffer) {
             strncpy_s(m_urlBuffer, currentUrl.c_str(), sizeof(m_urlBuffer) - 1);
        }

        // Display loading status or page title
        if (isLoading) {
             ImGui::Text("Loading: %s", currentUrl.c_str());
        } else {
             std::string title = mgr->GetTitle();
             ImGui::Text("Title: %s", title.empty() ? currentUrl.c_str() : title.c_str());
        }
    } else {
         ImGui::Text("Browser not available.");
    }
}

void BrowserPage::RenderBrowserView() {
    // Calculate available space for browser view, leaving room for controls/bookmarks
    // Use ImGui::GetContentRegionAvail() for dynamic sizing within the current window/child
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    // Example: If bookmarks section is fixed height, subtract it.
    // float bookmarksHeight = 100.0f;
    // ImVec2 viewSize = ImVec2(contentRegion.x, contentRegion.y - bookmarksHeight - ImGui::GetStyle().ItemSpacing.y);
    ImVec2 viewSize = contentRegion; // Use full available space for now

    // Ensure minimum size
    viewSize.x = std::max(viewSize.x, 64.0f);
    viewSize.y = std::max(viewSize.y, 64.0f);

    // Render browser content texture within a child window for clipping/scrolling if needed
    // ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    // ImGui::BeginChild("BrowserViewChild", viewSize, false, flags);

    if (m_browserView) {
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_browserView->GetTextureGpuHandle();
        if (gpuHandle.ptr != 0) {
            // Texture exists, render it using ImGui::Image
            ImGui::Image(
                reinterpret_cast<ImTextureID>(gpuHandle.ptr), // Cast GPU handle
                viewSize // Use calculated size
            );
        } else {
            // Texture handle is invalid (not created or descriptor issue)
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Browser texture not ready.");
            // Display a placeholder rectangle
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1 = ImVec2(p0.x + viewSize.x, p0.y + viewSize.y);
            draw_list->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 200));
            ImGui::Dummy(viewSize); // Advance cursor
        }
    } else {
        // BrowserView object itself is null
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Browser component is unavailable.");
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + viewSize.x, p0.y + viewSize.y);
        draw_list->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 200));
        ImGui::Dummy(viewSize); // Advance cursor
    }

    // ImGui::EndChild(); // End child window if used
}

void BrowserPage::RenderBookmarksSection() {
    // Example: Fixed height child window at the bottom
    float bookmarksBarHeight = 80.0f;
     // Use BeginChild for a dedicated area, helps with layout
    if (ImGui::BeginChild("BookmarksBarChild", ImVec2(0, bookmarksBarHeight), true, ImGuiWindowFlags_HorizontalScrollbar)) {

        ImGui::Text("Bookmarks");
        ImGui::Separator();
        ImGui::Spacing();

        // Render bookmarks as horizontal buttons
        float buttonWidth = 120.0f; // Adjust width as needed
        for (size_t i = 0; i < m_bookmarks.size(); ++i) {
            if (i > 0) ImGui::SameLine(); // Place buttons horizontally

            const auto& bm = m_bookmarks[i];
            std::string label = bm.favicon + " " + bm.name;

            if (ImGui::Button(label.c_str(), ImVec2(buttonWidth, 0))) {
                LoadBookmark(bm.url);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", bm.url.c_str());
            }
        }
    }
    ImGui::EndChild();
}

// --- Bookmark Dialog ---
void BrowserPage::RenderBookmarkDialog() {
    // Center the popup modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // Set initial size constraint
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Bookmark Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_isAddingBookmark) {
            ImGui::Text("Add Bookmark");
            ImGui::Separator();
            ImGui::Spacing();

            std::string currentUrl = "";
            if (m_browserView && m_browserView->GetBrowserManager()) {
                currentUrl = m_browserView->GetBrowserManager()->GetURL();
                std::string currentTitle = m_browserView->GetBrowserManager()->GetTitle();

                // Pre-fill name buffer only if it's currently empty
                if (strlen(m_bookmarkNameBuffer) == 0 && !currentTitle.empty()) {
                     strncpy_s(m_bookmarkNameBuffer, currentTitle.c_str(), sizeof(m_bookmarkNameBuffer) - 1);
                }
            }

            ImGui::InputText("Name", m_bookmarkNameBuffer, sizeof(m_bookmarkNameBuffer));
            ImGui::Text("URL: %s", currentUrl.c_str()); // Display URL, don't allow editing here
            // Add Favicon selection if needed

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (strlen(m_bookmarkNameBuffer) > 0 && !currentUrl.empty()) {
                    SaveCurrentPageAsBookmark();
                    ImGui::CloseCurrentPopup();
                } else {
                     // Optional: Show error message if name/URL is missing
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

        } else { // Managing bookmarks
            ImGui::Text("Manage Bookmarks");
            ImGui::Separator();
            ImGui::Spacing();

            // Table for listing bookmarks
            if (ImGui::BeginTable("BookmarksTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();

                // Use index for safe deletion while iterating
                for (int i = 0; i < static_cast<int>(m_bookmarks.size()); ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s %s", m_bookmarks[i].favicon.c_str(), m_bookmarks[i].name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_bookmarks[i].url.c_str());
                    ImGui::TableNextColumn();
                    ImGui::PushID(i); // Ensure unique ID for buttons within the loop
                    if (ImGui::Button("Delete")) {
                        DeleteBookmark(i);
                        ImGui::PopID();
                        break; // Exit loop after deletion as indices shift
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}


// --- Bookmark Logic ---

void BrowserPage::SaveCurrentPageAsBookmark() {
    if (!m_browserView || !m_browserView->GetBrowserManager()) return;

    std::string name = m_bookmarkNameBuffer;
    std::string url = m_browserView->GetBrowserManager()->GetURL();
    std::string icon = "🔖"; // Default icon, could be made selectable

    // Use page title as name if buffer is empty
    if (name.empty()) {
        name = m_browserView->GetBrowserManager()->GetTitle();
        // Use URL domain as fallback if title is also empty
        if (name.empty() && !url.empty()) {
             size_t start = url.find("://");
             if (start != std::string::npos) start += 3; else start = 0;
             size_t end = url.find('/', start);
             name = (end == std::string::npos) ? url.substr(start) : url.substr(start, end - start);
        }
        // Final fallback
        if (name.empty()) name = "Unnamed Bookmark";
    }

    // Don't add if URL is empty or invalid (e.g., about:blank)
    if (url.empty() || url == "about:blank" || url.find("data:") == 0) return;

    // Check if bookmark with this URL already exists
    auto it = std::find_if(m_bookmarks.begin(), m_bookmarks.end(),
                           [&url](const Bookmark& bm) { return bm.url == url; });

    if (it != m_bookmarks.end()) {
        // Update existing bookmark's name and icon
        it->name = name;
        it->favicon = icon;
    } else {
        // Add new bookmark
        m_bookmarks.push_back({ name, url, icon });
    }

    // Clear the input buffer after saving
    m_bookmarkNameBuffer[0] = '\0';
}

void BrowserPage::LoadBookmark(const std::string& url) {
    if (m_browserView) {
        m_browserView->Navigate(url);
        // Update URL buffer in UI
         strncpy_s(m_urlBuffer, url.c_str(), sizeof(m_urlBuffer) - 1);
    }
}

void BrowserPage::DeleteBookmark(size_t index) {
    if (index < m_bookmarks.size()) {
        m_bookmarks.erase(m_bookmarks.begin() + index);
    }
}