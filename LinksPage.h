// GameOverlay - LinksPage.h
// Phase 3: User Interface Development
// Links page with customizable link categories

#pragma once

#include "PageBase.h"
#include <string>
#include <vector>
#include <map>

class LinksPage : public PageBase {
public:
    LinksPage();
    ~LinksPage() = default;

    // Render links page content
    void Render() override;

private:
    // Render category management section
    void RenderCategoryManagement();

    // Render links for each category
    void RenderCategoryLinks();

    // Add/edit/delete functionality
    void AddCategory(const std::string& name);
    void RenameCategory(const std::string& oldName, const std::string& newName);
    void DeleteCategory(const std::string& name);
    void AddLink(const std::string& category, const std::string& name, const std::string& url, const std::string& icon);
    void DeleteLink(const std::string& category, int linkIndex);

    // Link data structure
    struct Link {
        std::string name;
        std::string url;
        std::string icon;
    };

    // Map of category name to array of links
    std::map<std::string, std::vector<Link>> m_categories;

    // UI state
    char m_categoryBuffer[256] = {};
    char m_linkNameBuffer[256] = {};
    char m_linkUrlBuffer[1024] = {};
    char m_linkIconBuffer[64] = {};
    std::string m_currentCategory;
    bool m_showAddLinkDialog = false;
};