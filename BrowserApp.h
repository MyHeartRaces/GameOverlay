// GameOverlay - BrowserApp.h
// Phase 2: CEF Integration
// CEF application implementation

#pragma once

#include "cef_app.h"

class BrowserApp : public CefApp,
    public CefBrowserProcessHandler,
    public CefRenderProcessHandler {
public:
    BrowserApp();

    // CefApp methods
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    // CefBrowserProcessHandler methods
    void OnContextInitialized() override;

    // CefRenderProcessHandler methods
    void OnWebKitInitialized() override;
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override;

private:
    // Include CefBase ref counting
    IMPLEMENT_REFCOUNTING(BrowserApp);
};