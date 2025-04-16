// GameOverlay - PageBase.h
// Phase 3: User Interface Development
// Base class for UI pages

#pragma once

#include <string>

// Base class for all UI pages
class PageBase {
public:
    PageBase(const std::string& name) : m_name(name) {}
    virtual ~PageBase() = default;

    // Render page content
    virtual void Render() = 0;

    // Get page name
    const std::string& GetName() const { return m_name; }

protected:
    std::string m_name;

    // Helper methods for common UI elements
    void RenderSectionHeader(const char* label);
    void RenderSeparator();
    void RenderInfoText(const char* text);
    void RenderWarningText(const char* text);
    void RenderErrorText(const char* text);
};