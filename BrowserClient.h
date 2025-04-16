// GameOverlay - BrowserClient.h
// Phase 2: CEF Integration
// CEF client implementation for browser instance

#pragma once

#include "cef_client.h"
#include "BrowserHandler.h"

class BrowserClient : public CefClient {
public:
    BrowserClient(CefRefPtr<BrowserHandler> handler);

    // Handler getters
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return m_handler; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return m_handler; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return m_handler; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return m_handler; }

    // JavaScript dialog handling - suppress dialogs
    CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
    bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url,
        JSDialogType dialog_type, const CefString& message_text,
        const CefString& default_prompt_text, CefRefPtr<CefJSDialogCallback> callback,
        bool& suppress_message) override;

    // Context menu handling - disable context menu
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override;

private:
    CefRefPtr<BrowserHandler> m_handler;

    // Include CefBase ref counting
    IMPLEMENT_REFCOUNTING(BrowserClient);
};