// GameOverlay - BrowserHandler.cpp
// Phase 2: CEF Integration
// Handles browser events and rendering callbacks

#include "BrowserHandler.h"
#include <d3d11.h>

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

        // Copy browser rendering to shared DirectX texture
        ID3D11Texture2D* texture = static_cast<ID3D11Texture2D*>(m_sharedTexture);
        if (!texture) return;

        // Get texture description to verify size
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        // Get device context associated with texture
        ID3D11Device* device = nullptr;
        texture->GetDevice(&device);
        if (!device) return;

        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);
        if (!context) {
            device->Release();
            return;
        }

        // Map texture for writing
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            // Calculate source and destination stride
            int src_stride = width * 4; // BGRA = 4 bytes per pixel
            int dst_stride = mapped.RowPitch;

            // Copy pixel data row by row
            const unsigned char* src = static_cast<const unsigned char*>(buffer);
            unsigned char* dst = static_cast<unsigned char*>(mapped.pData);

            for (int i = 0; i < height && i < (int)desc.Height; ++i) {
                memcpy(dst, src, std::min(src_stride, dst_stride));
                src += src_stride;
                dst += dst_stride;
            }

            // Unmap the texture
            context->Unmap(texture, 0);
        }

        // Release resources
        context->Release();
        device->Release();
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
