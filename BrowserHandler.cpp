// GameOverlay - BrowserHandler.cpp
// Handles browser events and rendering callbacks for DirectX 12

#include "BrowserHandler.h"
#include "BrowserManager.h" // Include manager for signalling
#include <d3d12.h>          // Keep includes potentially needed

BrowserHandler::BrowserHandler() {}

void BrowserHandler::SetBrowserManager(BrowserManager* manager) {
    m_browserManager = manager;
}

void BrowserHandler::SetBrowserSize(int width, int height) {
    m_width = width;
    m_height = height;
    // Note: CefBrowserHost::WasResized needs to be called externally
    // when the logical size changes.
}

std::string BrowserHandler::GetTitle() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_title;
}

bool BrowserHandler::IsLoading() const {
    return m_isLoading;
}

// --- CefRenderHandler methods ---

bool BrowserHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    // No lock needed for atomics
    rect.x = 0;
    rect.y = 0;
    rect.width = m_width;
    rect.height = m_height;
    return true;
}

void BrowserHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
    const RectList& dirtyRects, const void* buffer,
    int width, int height) {
    // We only care about the main view paint events
    if (type == PET_VIEW && buffer && m_browserManager) {
        // Directly call the manager's OnPaint method
        // This decouples the handler from the specific texture update mechanism
        m_browserManager->OnPaint(buffer, width, height);
    }
}

// --- CefLifeSpanHandler methods ---

void BrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    m_browserCreated = true;
}

bool BrowserHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    // Allow the close to proceed. The browser object will be released.
    return false;
}

void BrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    m_browserCreated = false;
    m_isLoading = false;
    // Reset any other relevant state if needed
}

// --- CefLoadHandler methods ---

void BrowserHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    TransitionType transition_type) {
    if (frame->IsMain()) {
        m_isLoading = true;
    }
}

void BrowserHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    int httpStatusCode) {
    if (frame->IsMain()) {
        m_isLoading = false;
    }
}

void BrowserHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    ErrorCode errorCode, const CefString& errorText,
    const CefString& failedUrl) {
    if (frame->IsMain()) {
        m_isLoading = false;

        // Don't display an error page if the user initiated the stop
        if (errorCode == ERR_ABORTED) {
            return;
        }

        // Display error page within the frame
        std::string error_html = "<html><body bgcolor=\"#F0F0F0\">"
            "<h2>Page Load Error</h2>"
            "<p>Failed to load URL: " + failedUrl.ToString() + "</p>"
            "<p>Error: " + errorText.ToString() +
            " (Code: " + std::to_string(errorCode) + ")</p>"
            "</body></html>";
        frame->LoadString(error_html, "data:text/html,chromewebdata");
    }
}

// --- CefDisplayHandler methods ---

void BrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_title = title.ToString();
}
