// GameOverlay - BrowserManager.cpp
// Phase 2: CEF Integration
// Manages CEF initialization and browser instances

#include "BrowserManager.h"
#include "BrowserApp.h"
#include "BrowserClient.h"
#include <sstream>
#include <filesystem>

BrowserManager::BrowserManager()
    : m_browserHandler(new BrowserHandler()) {
}

BrowserManager::~BrowserManager() {
    Shutdown();
}

bool BrowserManager::Initialize(HINSTANCE hInstance) {
    // Check if we're a CEF subprocess
    CefMainArgs main_args(hInstance);

    // Create the CefApp for the browser process
    CefRefPtr<BrowserApp> app(new BrowserApp());

    // Check if this is a subprocess that needs to run CEF's logic and exit
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        // This is a subprocess - exit with returned code
        m_isSubprocess = true;
        return false;
    }

    // Initialize CEF settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    settings.windowless_rendering_enabled = true;

    // Enable remote debugging on port 8088
    settings.remote_debugging_port = 8088;

    // Use in-memory cache to reduce disk I/O
    settings.persist_session_cookies = false;
    settings.persist_user_preferences = false;

    // Get current executable path for subprocess
    char szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, MAX_PATH);
    CefString(&settings.browser_subprocess_path).FromASCII(szPath);

    // Initialize CEF
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return false;
    }

    m_initialized = true;
    return true;
}

void BrowserManager::Shutdown() {
    // Close any existing browser
    CloseBrowser(true);

    // Shut down CEF if initialized
    if (m_initialized) {
        CefShutdown();
        m_initialized = false;
    }
}

bool BrowserManager::CreateBrowser(const std::string& url) {
    if (!m_initialized) return false;

    // Close any existing browser
    CloseBrowser(true);

    // Configure window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr); // No parent window needed for off-screen rendering

    // Browser settings
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60; // 60 fps

    // Set javascript flags to reduce memory usage
    CefString(&browser_settings.javascript_flags).FromASCII("--expose-gc --max-old-space-size=128");

    // Disable web security for overlay functionality
    browser_settings.web_security = STATE_DISABLED;

    // Create browser client
    CefRefPtr<BrowserClient> client(new BrowserClient(m_browserHandler));

    // Create the browser synchronously
    m_browser = CefBrowserHost::CreateBrowserSync(
        window_info,
        client,
        url,
        browser_settings,
        nullptr,
        nullptr
    );

    if (!m_browser) return false;

    // Set browser size for off-screen rendering
    m_browser->GetHost()->WasResized();

    return true;
}

void BrowserManager::CloseBrowser(bool forceClose) {
    if (m_browser) {
        m_browser->GetHost()->CloseBrowser(forceClose);
        m_browser = nullptr;
    }
}

void BrowserManager::LoadURL(const std::string& url) {
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL(url);
    }
}

void BrowserManager::GoBack() {
    if (m_browser) {
        m_browser->GoBack();
    }
}

void BrowserManager::GoForward() {
    if (m_browser) {
        m_browser->GoForward();
    }
}

void BrowserManager::Reload(bool ignoreCache) {
    if (m_browser) {
        if (ignoreCache) {
            m_browser->ReloadIgnoreCache();
        }
        else {
            m_browser->Reload();
        }
    }
}

void BrowserManager::StopLoad() {
    if (m_browser) {
        m_browser->StopLoad();
    }
}

bool BrowserManager::IsLoading() const {
    return m_browser ? m_browser->IsLoading() : false;
}

bool BrowserManager::CanGoBack() const {
    return m_browser ? m_browser->CanGoBack() : false;
}

bool BrowserManager::CanGoForward() const {
    return m_browser ? m_browser->CanGoForward() : false;
}

std::string BrowserManager::GetURL() const {
    if (!m_browser) return "";
    return m_browser->GetMainFrame()->GetURL().ToString();
}

std::string BrowserManager::GetTitle() const {
    if (!m_browserHandler) return "";
    return m_browserHandler->GetTitle();
}

void BrowserManager::DoMessageLoopWork() {
    if (m_initialized) {
        CefDoMessageLoopWork();
    }
}

void BrowserManager::OnPaint(void* shared_texture) {
    if (m_browserHandler) {
        m_browserHandler->SetSharedTexture(shared_texture);
    }
}