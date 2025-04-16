// GameOverlay - WindowManager.cpp
// Phase 1: Foundation Framework
// Manages the transparent overlay window

#include "WindowManager.h"
#include <stdexcept>

WindowManager::WindowManager(HINSTANCE hInstance, WNDPROC windowProc) {
    RegisterWindowClass(hInstance, windowProc);
    CreateOverlayWindow(hInstance);
}

WindowManager::~WindowManager() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    // Unregister window class
    UnregisterClass(m_windowClassName.c_str(), GetModuleHandle(NULL));
}

void WindowManager::RegisterWindowClass(HINSTANCE hInstance, WNDPROC windowProc) {
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = windowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr; // No background for transparent window
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = m_windowClassName.c_str();
    wcex.hIconSm = nullptr;

    if (!RegisterClassEx(&wcex)) {
        throw std::runtime_error("Failed to register window class");
    }
}

void WindowManager::CreateOverlayWindow(HINSTANCE hInstance) {
    // Get screen dimensions for fullscreen overlay
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Update member variables
    m_width = screenWidth;
    m_height = screenHeight;

    // Create a layered window for transparency
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
    DWORD style = WS_POPUP | WS_VISIBLE;

    // Create the window
    m_hwnd = CreateWindowEx(
        exStyle,
        m_windowClassName.c_str(),
        m_windowTitle.c_str(),
        style,
        0, 0,
        m_width, m_height,
        nullptr,
        nullptr,
        hInstance,
        this  // Pass pointer to this instance
    );

    if (!m_hwnd) {
        throw std::runtime_error("Failed to create overlay window");
    }

    // Set the layered window attributes for transparency
    // Using LWA_COLORKEY for color-based transparency
    SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // Show the window
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void WindowManager::HandleResize(int width, int height) {
    if (width <= 0 || height <= 0) return;

    m_width = width;
    m_height = height;

    // Resize the window
    SetWindowPos(
        m_hwnd,
        nullptr,
        0, 0,
        m_width, m_height,
        SWP_NOMOVE | SWP_NOZORDER
    );
}

void WindowManager::SetActive(bool active) {
    m_isActive = active;

    // When active: Allow input to pass through to the window
    // When inactive: Make window click-through (transparent to mouse input)
    LONG exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);

    if (active) {
        // Remove WS_EX_TRANSPARENT flag to receive mouse input
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    else {
        // Add WS_EX_TRANSPARENT flag to pass mouse input through
        exStyle |= WS_EX_TRANSPARENT;
    }

    SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle);
}

void WindowManager::SetVisible(bool visible) {
    m_isVisible = visible;

    // Show or hide the window
    if (visible) {
        ShowWindow(m_hwnd, SW_SHOW);
    }
    else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}
