// GameOverlay - BrowserClient.cpp
// Phase 2: CEF Integration
// CEF client implementation for browser instance

#include "BrowserClient.h"

BrowserClient::BrowserClient(CefRefPtr<BrowserHandler> handler)
    : m_handler(handler) {
}

bool BrowserClient::OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url,
    JSDialogType dialog_type, const CefString& message_text,
    const CefString& default_prompt_text, CefRefPtr<CefJSDialogCallback> callback,
    bool& suppress_message) {
    // Suppress all JavaScript dialogs since they're not appropriate for an overlay
    suppress_message = true;

    // If there's a callback, let it continue
    if (callback) {
        callback->Continue(false, CefString());
    }

    return true;
}

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) {
    // Clear the menu to disable context menu
    model->Clear();
}
