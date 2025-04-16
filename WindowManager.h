// GameOverlay - WindowManager.h
// Phase 1: Foundation Framework
// Manages the transparent overlay window

#pragma once

#include <Windows.h>
#include <string>
#include <functional>

class WindowManager {
public:
    WindowManager(HINSTANCE hInstance, WNDPROC windowProc);
    ~WindowManager();

    // Disable copy and move
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&) = delete;
    WindowManager& operator=(WindowManager&&) = delete;

    // Getters
    HWND GetHWND() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Handle window resize
    void HandleResize(int width, int height);

    // Window state management
    void SetActive(bool active);
    bool IsActive() const { return m_isActive; }

    // Visibility management
    void SetVisible(bool visible);
    bool IsVisible() const { return m_isVisible; }

private:
    void RegisterWindowClass(HINSTANCE hInstance, WNDPROC windowProc);
    void CreateOverlayWindow(HINSTANCE hInstance);

    HWND m_hwnd = nullptr;
    int m_width = 1280;
    int m_height = 720;
    std::wstring m_windowClassName = L"GameOverlayWindowClass";
    std::wstring m_windowTitle = L"GameOverlay";
    bool m_isActive = true;
    bool m_isVisible = true;
};