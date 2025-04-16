// GameOverlay - BrowserManager.h
// Phase 2: CEF Integration
// Manages CEF initialization and browser instances

#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include "cef_app.h"
#include "cef_client.h"
#include "cef_browser.h"
#include "BrowserHandler.h"

class BrowserManager {
public:
    BrowserManager();
    ~BrowserManager();

    // Disable copy and move
    BrowserManager(const BrowserManager&) = delete;
    BrowserManager& operator=(const BrowserManager&) = delete;
    BrowserManager(BrowserManager&&) = delete;
    BrowserManager& operator=(BrowserManager&&) = delete;

    // CEF initialization
    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Browser management
    bool CreateBrowser(const std::string& url);
    void CloseBrowser(bool forceClose = false);

    // Browser interaction
    void LoadURL(const std::string& url);
    void GoBack();
    void GoForward();
    void Reload(bool ignoreCache = false);
    void StopLoad();

    // Browser state
    bool IsLoading() const;
    bool CanGoBack() const;
    bool CanGoForward() const;
    std::string GetURL() const;
    std::string GetTitle() const;

    // CEF process handling
    void DoMessageLoopWork();

    // Rendering
    void OnPaint(void* shared_texture);
    unsigned int GetBrowserWidth() const { return m_browserWidth; }
    unsigned int GetBrowserHeight() const { return m_browserHeight; }

    // Handler access
    BrowserHandler* GetBrowserHandler() { return m_browserHandler.get(); }

private:
    // Initialize CEF subprocess
    bool InitializeSubprocess();

    // CEF objects
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<BrowserHandler> m_browserHandler;

    // State
    bool m_initialized = false;
    unsigned int m_browserWidth = 1024;
    unsigned int m_browserHeight = 768;

    // Subprocess handling
    bool m_isSubprocess = false;
};