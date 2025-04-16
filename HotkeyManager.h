// GameOverlay - HotkeyManager.h
// Phase 4: Hotkey System Implementation
// Global hotkey management system

#pragma once

#include <Windows.h>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <mutex>

// Forward declarations
class WindowManager;

// Represents a keyboard key combination (modifiers + key)
struct Hotkey {
    bool ctrl;
    bool alt;
    bool shift;
    bool win;
    DWORD key;

    // Constructor with default values
    Hotkey(DWORD keyCode = 0, bool ctrlKey = false, bool altKey = false, bool shiftKey = false, bool winKey = false)
        : key(keyCode), ctrl(ctrlKey), alt(altKey), shift(shiftKey), win(winKey) {
    }

    // Compare hotkeys for equality
    bool operator==(const Hotkey& other) const {
        return key == other.key &&
            ctrl == other.ctrl &&
            alt == other.alt &&
            shift == other.shift &&
            win == other.win;
    }

    // Check if this hotkey is empty (no key assigned)
    bool IsEmpty() const {
        return key == 0;
    }

    // Convert hotkey to string representation
    std::string ToString() const;

    // Parse string to hotkey
    static Hotkey FromString(const std::string& str);
};

// HotkeyManager class for global hotkey handling
class HotkeyManager {
public:
    HotkeyManager(WindowManager* windowManager);
    ~HotkeyManager();

    // Disable copy and move
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    HotkeyManager(HotkeyManager&&) = delete;
    HotkeyManager& operator=(HotkeyManager&&) = delete;

    // Action type (callback function)
    using HotkeyAction = std::function<void()>;

    // Register and unregister hotkeys
    bool RegisterHotkey(const std::string& actionName, const Hotkey& hotkey, HotkeyAction action);
    bool UnregisterHotkey(const std::string& actionName);
    bool UpdateHotkey(const std::string& actionName, const Hotkey& hotkey);

    // Handle key events
    bool ProcessKeyEvent(WPARAM wParam, LPARAM lParam);

    // Get registered hotkeys
    std::map<std::string, Hotkey> GetHotkeys() const;

    // Check if a key combination is already registered
    bool IsHotkeyRegistered(const Hotkey& hotkey) const;

    // Hook installation and removal
    bool InstallHook();
    void RemoveHook();

    // Process a keyboard message from the global hook
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    // Static instance for hook callback
    static HotkeyManager* s_instance;

    // Windows hook handle
    HHOOK m_keyboardHook = NULL;

    // Map of action names to hotkeys and actions
    std::map<std::string, std::pair<Hotkey, HotkeyAction>> m_hotkeyMap;

    // Current state of modifier keys
    bool m_ctrlDown = false;
    bool m_altDown = false;
    bool m_shiftDown = false;
    bool m_winDown = false;

    // Pointer to window manager (not owned)
    WindowManager* m_windowManager = nullptr;

    // Thread safety
    mutable std::mutex m_mutex;

    // Check if the key pressed matches any registered hotkey
    bool CheckHotkeys(DWORD keyCode, bool keyUp);

    // Helper to update modifier state
    void UpdateModifierState(DWORD keyCode, bool keyDown);

    // Register standard default hotkeys
    void RegisterDefaultHotkeys();
};