// GameOverlay - BrowserHandler.cpp
// Phase 6: DirectX 12 Migration
// Handles browser events and rendering callbacks for DirectX 12

#include "BrowserHandler.h"
#include <d3d12.h>

BrowserHandler::BrowserHandler() {
}

void BrowserHandler::SetBrowserSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_width = width;
    m_height = height;
}

bool BrowserHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    std::lock_guard<std::mutex> lock(m_mutex);
    rect.x = 0;
    rect.y = 0;
    rect.width = m_width;
    rect.height = m_height;
    return true;
}

void BrowserHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
    const RectList& dirtyRects, const void* buffer,
    int width, int height) {
    if (type == PET_VIEW && m_sharedTexture && buffer) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // For DirectX 12, this would be an ID3D12Resource in an upload heap
        ID3D12Resource* uploadBuffer = static_cast<ID3D12Resource*>(m_sharedTexture);
        if (!uploadBuffer) return;

        // Map the upload buffer for CPU write
        D3D12_RANGE readRange = { 0, 0 }; // We don't intend to read
        void* mappedData = nullptr;
        HRESULT hr = uploadBuffer->Map(0, &readRange, &mappedData);

        if (SUCCEEDED(hr)) {
            // Calculate source and destination dimensions
            size_t rowPitch = (width * 4 + 255) & ~255; // Align to 256 bytes for DirectX 12

            // Copy the browser pixel data to the upload buffer
            // The browser buffer is tightly packed BGRA data
            const unsigned char* src = static_cast<const unsigned char*>(buffer);
            unsigned char* dst = static_cast<unsigned char*>(mappedData);

            // Copy row by row
            for (int i = 0; i < height; i++) {
                memcpy(dst + i * rowPitch, src + i * width * 4, width * 4);
            }

            // Calculate range of written data for proper unmapping
            D3D12_RANGE writtenRange = { 0, height * rowPitch };
            uploadBuffer->Unmap(0, &writtenRange);
        }
    }
}

void BrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_browserCreated = true;
}

bool BrowserHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    return false; // Allow the close to proceed
}

void BrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_browserCreated = false;
}

void BrowserHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    TransitionType transition_type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frame->IsMain()) {
        m_isLoading = true;
    }
}

void BrowserHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    int httpStatusCode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frame->IsMain()) {
        m_isLoading = false;
    }
}

void BrowserHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    ErrorCode errorCode, const CefString& errorText,
    const CefString& failedUrl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frame->IsMain()) {
        m_isLoading = false;

        // Display error page
        std::string error_html = "<html><body style='background-color: #f0f0f0;'>"
            "<h2>Error loading page</h2>"
            "<p>Failed to load: " + failedUrl.ToString() + "</p>"
            "<p>Error: " + errorText.ToString() + " (" + std::to_string(errorCode) + ")</p>"
            "</body></html>";

        frame->LoadString(error_html, failedUrl);
    }
}

void BrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_title = title.ToString();
}
