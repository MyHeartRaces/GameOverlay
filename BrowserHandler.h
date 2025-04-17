// GameOverlay - BrowserHandler.h
// Phase 6: DirectX 12 Migration
// Handles browser events and rendering callbacks for DirectX 12

#pragma once

#include "cef_client.h"
#include "cef_render_handler.h"
#include "cef_life_span_handler.h"
#include "cef_load_handler.h"
#include "cef_display_handler.h"
#include <d3d12.h>
#include <string>
#include <mutex>

class BrowserHandler : public CefRenderHandler,
    public CefLifeSpanHandler,
    public CefLoadHandler,
    public CefDisplayHandler {
public:
    BrowserHandler();

    // Shared texture for DirectX 12 integration
    void SetSharedTexture(void* texture) { m_sharedTexture = texture; }
    void* GetSharedTexture() const { return m_sharedTexture; }

    // Browser state access
    std::string GetTitle() const { return m_title; }
    void SetBrowserSize(int width, int height);

    // CefRenderHandler methods
    bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
        const RectList& dirtyRects, const void* buffer,
        int width, int height) override;

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler methods
    void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        int httpStatusCode) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        ErrorCode errorCode, const CefString& errorText,
        const CefString& failedUrl) override;

    // CefDisplayHandler methods
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

private:
    // Shared texture for DirectX 12 integration
    void* m_sharedTexture = nullptr;

    // Browser state
    std::string m_title;
    bool m_isLoading = false;
    bool m_browserCreated = false;

    // Browser dimensions
    int m_width = 1024;
    int m_height = 768;

    // Thread safety
    std::mutex m_mutex;

    // Include CefBase ref counting
    IMPLEMENT_REFCOUNTING(BrowserHandler);
};