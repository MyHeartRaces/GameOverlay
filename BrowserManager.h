// GameOverlay - BrowserManager.h
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

// Forward declaration
class BrowserView;

class BrowserManager {
public:
    // Pass BrowserView for signalling texture updates
    BrowserManager(BrowserView* view);
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

    // Rendering - Called by BrowserHandler's OnPaint via BrowserClient
    void OnPaint(const void* buffer, int width, int height);
    unsigned int GetBrowserWidth() const; // Use handler's width
    unsigned int GetBrowserHeight() const; // Use handler's height

    // Handler access
    BrowserHandler* GetBrowserHandler() { return m_browserHandler.get(); }

private:
    // Initialize CEF subprocess
    bool InitializeSubprocess();

    // CEF objects
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<BrowserHandler> m_browserHandler;
    CefRefPtr<BrowserClient> m_browserClient; // Keep ref to client

    // State
    bool m_initialized = false;

    // Subprocess handling
    bool m_isSubprocess = false;

    // Pointer back to BrowserView (not owned) to signal updates
    BrowserView* m_browserView = nullptr;
};