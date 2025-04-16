// GameOverlay - LinksPage.cpp
// Phase 3: User Interface Development
// Links page with customizable link categories

#include "LinksPage.h"
#include "imgui.h"
#include <algorithm>

LinksPage::LinksPage() : PageBase("Links") {
    // Initialize with some example categories and links
    m_categories["Gaming"] = {
        { "Steam", "https://store.steampowered.com", "🎮" },
        { "Epic Games", "https://www.epicgames.com", "🎮" },
        { "Twitch", "https://www.twitch.tv", "📺" },
        { "Discord", "https://discord.com", "💬" }
    };

    m_categories["Social"] = {
        { "Reddit", "https://www.reddit.com", "🌐" },
        { "Twitter", "https://twitter.com", "🐦" },
        { "YouTube", "https://www.youtube.com", "📺" },
        { "Facebook", "https://www.facebook.com", "👥" }
    };

    m_categories["News"] = {
        { "CNN", "https://www.cnn.com", "📰" },
        { "BBC", "https://www.bbc.com", "📰" },
        { "The Guardian", "https://www.theguardian.com", "📰" },
        { "Reuters", "https://www.reuters.com", "📰" }
    };

    m_categories["Development"] = {
        { "GitHub", "https://github.com", "💻" },
        { "Stack Overflow", "https://stackoverflow.com", "❓" },
        { "MDN Web Docs", "https://developer.mozilla.org", "📚" },
        { "W3Schools", "https://www.w3schools.com", "🎓" }
    };
}

void LinksPage::Render() {
    ImGui::BeginChild("LinksPageScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Render category management section at the top
    RenderCategoryManagement();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render links organized by category
    RenderCategoryLinks();

    ImGui::EndChild();

    // Add link dialog
    if (m_showAddLinkDialog) {
        ImGui::OpenPopup("Add Link");
        m_showAddLinkDialog = false;
    }

    // Centered popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Add Link", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Add link to category: %s", m_currentCategory.c_str());
        ImGui::Separator();

        ImGui::InputText("Name", m_linkNameBuffer, sizeof(m_linkNameBuffer));
        ImGui::InputText("URL", m_linkUrlBuffer, sizeof(m_linkUrlBuffer));

        // Icon selection
        const char* icons[] = { "🌐", "📰", "📺", "💻", "🎮", "🎓", "📚", "❓", "💬", "👥", "🐦", "🔍" };
        static int iconIndex = 0;

        ImGui::Text("Icon:");
        ImGui::SameLine();

        if (ImGui::BeginCombo("##IconCombo", icons[iconIndex])) {
            for (int i = 0; i < IM_ARRAYSIZE(icons); i++) {
                bool isSelected = (iconIndex == i);
                if (ImGui::Selectable(icons[i], isSelected)) {
                    iconIndex = i;
                    strcpy_s(m_linkIconBuffer, icons[i]);
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        if (ImGui::Button("Add", ImVec2(120, 0))) {
            if (strlen(m_linkNameBuffer) > 0 && strlen(m_linkUrlBuffer) > 0) {
                // Add http:// if protocol is missing
                std::string url = m_linkUrlBuffer;
                if (url.find("://") == std::string::npos) {
                    url = "https://" + url;
                }

                // Add link to the category
                AddLink(m_currentCategory, m_linkNameBuffer, url, strlen(m_linkIconBuffer) > 0 ? m_linkIconBuffer : "🌐");

                // Clear buffers
                m_linkNameBuffer[0] = '\0';
                m_linkUrlBuffer[0] = '\0';
                m_linkIconBuffer[0] = '\0';

                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void LinksPage::RenderCategoryManagement() {
    RenderSectionHeader("Link Categories");

    // Category management
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##CategoryName", m_categoryBuffer, sizeof(m_categoryBuffer));

    ImGui::SameLine();

    if (ImGui::Button("Add Category")) {
        if (strlen(m_categoryBuffer) > 0) {
            AddCategory(m_categoryBuffer);
            m_categoryBuffer[0] = '\0'; // Clear buffer
        }
    }

    ImGui::Spacing();

    if (ImGui::BeginTable("CategoriesTable", 2, ImGuiTableFlags_BordersOuter)) {
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        for (auto& category : m_categories) {
            ImGui::TableNextRow();

            // Category name
            ImGui::TableNextColumn();
            ImGui::Text("%s (%zu links)", category.first.c_str(), category.second.size());

            // Category actions
            ImGui::TableNextColumn();
            ImGui::PushID(category.first.c_str());

            if (ImGui::Button("Add Link")) {
                m_currentCategory = category.first;
                m_linkNameBuffer[0] = '\0';
                m_linkUrlBuffer[0] = '\0';
                m_linkIconBuffer[0] = '\0';
                m_showAddLinkDialog = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Delete")) {
                // Store for deletion after loop
                std::string categoryToDelete = category.first;
                ImGui::OpenPopup("DeleteCategoryConfirm");
            }

            // Confirmation popup
            if (ImGui::BeginPopup("DeleteCategoryConfirm")) {
                ImGui::Text("Delete category '%s'?", category.first.c_str());
                ImGui::Text("This will delete all %zu links in this category.", category.second.size());
                ImGui::Separator();

                if (ImGui::Button("Yes", ImVec2(60, 0))) {
                    DeleteCategory(category.first);
                    ImGui::CloseCurrentPopup();
                    ImGui::PopID();
                    break; // Break out of the loop since we're modifying the map
                }

                ImGui::SameLine();

                if (ImGui::Button("No", ImVec2(60, 0))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void LinksPage::RenderCategoryLinks() {
    ImGui::BeginTabBar("CategoriesTabBar");

    for (auto& category : m_categories) {
        if (ImGui::BeginTabItem(category.first.c_str())) {
            RenderSectionHeader(category.first);

            // Create a grid of link buttons
            float buttonWidth = 160.0f;
            float buttonHeight = 70.0f;
            float windowWidth = ImGui::GetContentRegionAvail().x;
            int buttonsPerRow = std::max(1, static_cast<int>(windowWidth / buttonWidth));

            int linkIndex = 0;
            for (auto& link : category.second) {
                // Start new row when needed
                if (linkIndex % buttonsPerRow != 0) {
                    ImGui::SameLine();
                }

                // Link button
                ImGui::PushID(linkIndex);
                ImGui::BeginGroup();

                // Link button with icon
                std::string buttonLabel = link.icon + " " + link.name;
                if (ImGui::Button(buttonLabel.c_str(), ImVec2(buttonWidth - 10, buttonHeight - 20))) {
                    // In a real implementation, this would navigate to the URL
                    // For now, just show a tooltip with the URL
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", link.url.c_str());
                }

                // Delete button below the link
                if (ImGui::Button("Delete", ImVec2(buttonWidth - 10, 20))) {
                    DeleteLink(category.first, linkIndex);
                    ImGui::PopID();
                    ImGui::EndGroup();
                    break; // Break out of the loop since we're modifying the vector
                }

                ImGui::EndGroup();
                ImGui::PopID();

                linkIndex++;
            }

            // If no links in this category, show a message
            if (category.second.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No links in this category.");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Click 'Add Link' button above to add links.");
            }

            ImGui::EndTabItem();
        }
    }

    ImGui::EndTabBar();
}

void LinksPage::AddCategory(const std::string& name) {
    if (!name.empty() && m_categories.find(name) == m_categories.end()) {
        m_categories[name] = std::vector<Link>();
    }
}

void LinksPage::RenameCategory(const std::string& oldName, const std::string& newName) {
    if (oldName != newName && !newName.empty() &&
        m_categories.find(oldName) != m_categories.end() &&
        m_categories.find(newName) == m_categories.end()) {

        // Copy links from old category to new
        m_categories[newName] = m_categories[oldName];

        // Delete old category
        m_categories.erase(oldName);
    }
}

void LinksPage::DeleteCategory(const std::string& name) {
    m_categories.erase(name);
}

void LinksPage::AddLink(const std::string& category, const std::string& name, const std::string& url, const std::string& icon) {
    if (!category.empty() && !name.empty() && !url.empty() &&
        m_categories.find(category) != m_categories.end()) {

        m_categories[category].push_back({ name, url, icon });
    }
}

void LinksPage::DeleteLink(const std::string& category, int linkIndex) {
    if (m_categories.find(category) != m_categories.end() &&
        linkIndex >= 0 && linkIndex < m_categories[category].size()) {

        m_categories[category].erase(m_categories[category].begin() + linkIndex);
    }
}