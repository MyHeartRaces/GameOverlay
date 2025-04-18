// GameOverlay - BrowserManager.cpp
// Manages CEF initialization and browser instances

#include "BrowserManager.h"
#include "BrowserApp.h"
#include "BrowserClient.h"
#include "BrowserView.h" // Include for signalling
#include <sstream>
#include <filesystem>
#include <stdexcept> // Include for error checking

// BrowserManager Constructor
BrowserManager::BrowserManager(BrowserView* view)
    : m_browserHandler(new BrowserHandler()), // Initialize handler first
    m_browserClient(new BrowserClient(m_browserHandler)), // Initialize client with handler
    m_browserView(view) // Store pointer to BrowserView
{
    if (!m_browserView) {
        // Optional: Throw or log error if BrowserView is null
        // throw std::invalid_argument("BrowserView cannot be null in BrowserManager constructor");
    }
}

BrowserManager::~BrowserManager() {
    Shutdown();
}

bool BrowserManager::Initialize(HINSTANCE hInstance) {
    // Check if already initialized or in subprocess
    if (m_initialized || m_isSubprocess) {
        return m_initialized;
    }

    // Check if we're a CEF subprocess
    CefMainArgs main_args(hInstance);

    // Create the CefApp for the browser process
    CefRefPtr<BrowserApp> app(new BrowserApp());

    // Check if this is a subprocess that needs to run CEF's logic and exit
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        // This is a subprocess - exit with returned code
        m_isSubprocess = true;
        return false; // Indicate subprocess mode
    }

    // Initialize CEF settings
    CefSettings settings;
    settings.no_sandbox = true; // Required for many environments
    settings.multi_threaded_message_loop = false; // We run the loop manually
    settings.windowless_rendering_enabled = true; // Essential for overlays

    // Enable remote debugging on port 8088 (optional, useful for development)
    settings.remote_debugging_port = 8088;

    // Cache settings (adjust as needed)
    settings.persist_session_cookies = false;
    settings.persist_user_preferences = false;
    // CefString(&settings.cache_path) = "path/to/cache"; // Optional: Specify a cache path

    // Get current executable path for subprocess
    char szPath[MAX_PATH];
    if (GetModuleFileNameA(NULL, szPath, MAX_PATH) == 0) {
        // Handle error - unable to get executable path
        return false;
    }
    CefString(&settings.browser_subprocess_path).FromASCII(szPath);

    // Initialize CEF
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        // Handle error - CEF initialization failed
        return false;
    }

    m_initialized = true;
    return true;
}

void BrowserManager::Shutdown() {
    // Close any existing browser
    CloseBrowser(true);

    // Shut down CEF if initialized and not in subprocess
    if (m_initialized && !m_isSubprocess) {
        CefShutdown();
    }

    m_browser = nullptr;
    m_browserClient = nullptr; // Release client ref
    // m_browserHandler is ref-counted, CefClient handles its release

    m_initialized = false;
}

bool BrowserManager::CreateBrowser(const std::string& url) {
    if (!m_initialized || m_isSubprocess) return false;

    // Close any existing browser first
    CloseBrowser(true);

    // Configure window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr); // No parent window needed

    // Browser settings
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60; // Target FPS for rendering

    // Optional: Set background color (e.g., transparent)
    // browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);

    // Optional: Web preferences
    // CefString(&browser_settings.default_encoding).FromASCII("UTF-8");

    // Create the browser asynchronously (or use CreateBrowserSync if needed)
    // Using sync here based on original code structure
    m_browser = CefBrowserHost::CreateBrowserSync(
        window_info,
        m_browserClient, // Use the member client
        url,
        browser_settings,
        nullptr, // Extra info
        nullptr  // Request context
    );

    if (!m_browser) {
        // Handle error - Browser creation failed
        return false;
    }

    // Need to explicitly tell the host the initial size
    if (m_browser->GetHost()) {
        m_browser->GetHost()->WasResized();
    }

    return true;
}

void BrowserManager::CloseBrowser(bool forceClose) {
    if (m_browser && m_browser->GetHost()) {
        m_browser->GetHost()->CloseBrowser(forceClose);
    }
    // CefLifeSpanHandler::OnBeforeClose will set m_browser to null later
}

void BrowserManager::LoadURL(const std::string& url) {
    if (m_browser && m_browser->GetMainFrame()) {
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
    // Check handler first as browser might be closing
    return m_browserHandler && m_browserHandler->IsLoading();
}

bool BrowserManager::CanGoBack() const {
    return m_browser ? m_browser->CanGoBack() : false;
}

bool BrowserManager::CanGoForward() const {
    return m_browser ? m_browser->CanGoForward() : false;
}

std::string BrowserManager::GetURL() const {
    if (m_browser && m_browser->GetMainFrame()) {
        return m_browser->GetMainFrame()->GetURL().ToString();
    }
    return "";
}

std::string BrowserManager::GetTitle() const {
    // Get title from handler as it's updated via callbacks
    return m_browserHandler ? m_browserHandler->GetTitle() : "";
}

void BrowserManager::DoMessageLoopWork() {
    if (m_initialized && !m_isSubprocess) {
        CefDoMessageLoopWork();
    }
}

// This method is now called by BrowserHandler when OnPaint occurs
void BrowserManager::OnPaint(const void* buffer, int width, int height) {
    if (m_browserView && buffer) {
        // Signal BrowserView that new texture data is available in the upload buffer
        m_browserView->SignalTextureUpdateFromHandler(buffer, width, height);
    }
}

unsigned int BrowserManager::GetBrowserWidth() const {
    // Get dimensions from the handler, which holds the correct size
    return m_browserHandler ? m_browserHandler->GetWidth() : 0;
}

unsigned int BrowserManager::GetBrowserHeight() const {
    // Get dimensions from the handler
    return m_browserHandler ? m_browserHandler->GetHeight() : 0;
}