// GameOverlay - BrowserApp.cpp
// Phase 2: CEF Integration
// CEF application implementation

#include "BrowserApp.h"

BrowserApp::BrowserApp() {
}

void BrowserApp::OnContextInitialized() {
    // CEF browser context is initialized
}

void BrowserApp::OnWebKitInitialized() {
    // WebKit is initialized in the render process

    // Register native JS functions for browser<->application communication
    static const char* extension_code =
        "var gameoverlay = gameoverlay || {};"
        "(function() {"
        "  gameoverlay.sendMessage = function(name, message) {"
        "    native function SendMessage();"
        "    return SendMessage(name, message);"
        "  };"
        "})();";

    CefRegisterExtension("v8/gameoverlay", extension_code, nullptr);
}

void BrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
    // Browser context is created

    // Inject custom JS into the page
    const char* customCode =
        "console.log('GameOverlay browser initialized');"
        "window.gameOverlayVersion = '0.1.0';"
        "window.gameOverlayPhase = 'Phase 2: CEF Integration';";

    frame->ExecuteJavaScript(customCode, frame->GetURL(), 0);
}
