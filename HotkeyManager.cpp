// GameOverlay - HotkeyManager.cpp
// Phase 4: Hotkey System Implementation
// Global hotkey management system

#include "HotkeyManager.h"
#include "WindowManager.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// Static instance pointer for hook callback
HotkeyManager* HotkeyManager::s_instance = nullptr;

// Convert hotkey to string representation
std::string Hotkey::ToString() const {
    if (IsEmpty()) return "None";

    std::stringstream ss;
    if (ctrl) ss << "Ctrl+";
    if (alt) ss << "Alt+";
    if (shift) ss << "Shift+";
    if (win) ss << "Win+";

    // Get key name from Virtual-Key code
    char keyName[32] = {};
    UINT scanCode = MapVirtualKey(key, MAPVK_VK_TO_VSC);

    // For extended keys (arrows, numpad, etc.)
    switch (key) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END:
    case VK_INSERT: case VK_DELETE:
    case VK_DIVIDE:
        scanCode |= 0x100; // set extended bit
        break;
    }

    if (key >= VK_F1 && key <= VK_F24) {
        ss << "F" << (key - VK_F1 + 1);
    }
    else if (GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0) {
        ss << keyName;
    }
    else {
        // Fallback for keys that GetKeyNameTextA can't handle
        switch (key) {
        case VK_ESCAPE: ss << "Esc"; break;
        case VK_RETURN: ss << "Enter"; break;
        case VK_SPACE: ss << "Space"; break;
        case VK_TAB: ss << "Tab"; break;
        case VK_BACK: ss << "Backspace"; break;
        default:
            if (key >= 'A' && key <= 'Z') {
                ss << static_cast<char>(key);
            }
            else if (key >= '0' && key <= '9') {
                ss << static_cast<char>(key);
            }
            else {
                ss << "Key(" << key << ")";
            }
            break;
        }
    }

    return ss.str();
}

// Parse string to hotkey
Hotkey Hotkey::FromString(const std::string& str) {
    if (str == "None") return Hotkey();

    Hotkey result;
    std::string remaining = str;

    // Check for modifiers
    if (remaining.find("Ctrl+") != std::string::npos) {
        result.ctrl = true;
        remaining.erase(0, 5); // Remove "Ctrl+"
    }

    if (remaining.find("Alt+") != std::string::npos) {
        result.alt = true;
        remaining.erase(0, 4); // Remove "Alt+"
    }

    if (remaining.find("Shift+") != std::string::npos) {
        result.shift = true;
        remaining.erase(0, 6); // Remove "Shift+"
    }

    if (remaining.find("Win+") != std::string::npos) {
        result.win = true;
        remaining.erase(0, 4); // Remove "Win+"
    }

    // Parse key
    if (remaining.empty()) {
        return result;
    }

    // Handle function keys
    if (remaining.size() >= 2 && remaining[0] == 'F') {
        int fKeyNum = 0;
        if (sscanf_s(remaining.c_str() + 1, "%d", &fKeyNum) == 1 && fKeyNum >= 1 && fKeyNum <= 24) {
            result.key = VK_F1 + (fKeyNum - 1);
            return result;
        }
    }

    // Handle common named keys
    if (remaining == "Esc") result.key = VK_ESCAPE;
    else if (remaining == "Enter") result.key = VK_RETURN;
    else if (remaining == "Space") result.key = VK_SPACE;
    else if (remaining == "Tab") result.key = VK_TAB;
    else if (remaining == "Backspace") result.key = VK_BACK;
    else if (remaining.size() == 1) {
        // Single character keys
        char c = remaining[0];
        if (c >= 'A' && c <= 'Z') {
            result.key = c;
        }
        else if (c >= 'a' && c <= 'z') {
            result.key = toupper(c); // Convert to uppercase for VK code
        }
        else if (c >= '0' && c <= '9') {
            result.key = c;
        }
    }

    return result;
}

// Constructor
HotkeyManager::HotkeyManager(WindowManager* windowManager)
    : m_windowManager(windowManager) {

    // Store instance for static hook callback
    s_instance = this;

    // Install the keyboard hook
    InstallHook();

    // Register default hotkeys
    RegisterDefaultHotkeys();
}

// Destructor
HotkeyManager::~HotkeyManager() {
    // Remove hook
    RemoveHook();

    // Clear the static instance
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

// Register a hotkey with an action
bool HotkeyManager::RegisterHotkey(const std::string& actionName, const Hotkey& hotkey, HotkeyAction action) {
    if (hotkey.IsEmpty()) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if this hotkey is already used
    if (IsHotkeyRegistered(hotkey)) {
        // Already registered to something else
        for (const auto& [name, pair] : m_hotkeyMap) {
            if (pair.first == hotkey && name != actionName) {
                return false;
            }
        }
    }

    // Add or update the hotkey
    m_hotkeyMap[actionName] = std::make_pair(hotkey, action);
    return true;
}

// Unregister a hotkey by action name
bool HotkeyManager::UnregisterHotkey(const std::string& actionName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_hotkeyMap.find(actionName);
    if (it != m_hotkeyMap.end()) {
        m_hotkeyMap.erase(it);
        return true;
    }

    return false;
}

// Update an existing hotkey
bool HotkeyManager::UpdateHotkey(const std::string& actionName, const Hotkey& hotkey) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_hotkeyMap.find(actionName);
    if (it == m_hotkeyMap.end()) {
        return false;
    }

    // Check if this hotkey is already used
    if (!hotkey.IsEmpty() && IsHotkeyRegistered(hotkey)) {
        // Already registered to something else
        for (const auto& [name, pair] : m_hotkeyMap) {
            if (pair.first == hotkey && name != actionName) {
                return false;
            }
        }
    }

    // Update the hotkey, keeping the same action
    it->second.first = hotkey;
    return true;
}

// Process key events from the main window
bool HotkeyManager::ProcessKeyEvent(WPARAM wParam, LPARAM lParam) {
    bool keyDown = !(lParam & (1 << 31)); // Check if key is down
    DWORD keyCode = static_cast<DWORD>(wParam);

    // Update modifier state
    UpdateModifierState(keyCode, keyDown);

    // If key is pressed down, check hotkeys
    if (keyDown) {
        return CheckHotkeys(keyCode, false);
    }

    return false;
}

// Get all registered hotkeys
std::map<std::string, Hotkey> HotkeyManager::GetHotkeys() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::map<std::string, Hotkey> result;
    for (const auto& [name, pair] : m_hotkeyMap) {
        result[name] = pair.first;
    }

    return result;
}

// Check if a hotkey is already registered
bool HotkeyManager::IsHotkeyRegistered(const Hotkey& hotkey) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (hotkey.IsEmpty()) return false;

    for (const auto& [name, pair] : m_hotkeyMap) {
        if (pair.first == hotkey) {
            return true;
        }
    }

    return false;
}

// Install the keyboard hook
bool HotkeyManager::InstallHook() {
    if (m_keyboardHook != NULL) {
        return true; // Already installed
    }

    m_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,         // Low-level keyboard hook
        LowLevelKeyboardProc,   // Hook procedure
        GetModuleHandle(NULL),  // Current module instance
        0                       // Hook into all threads
    );

    return (m_keyboardHook != NULL);
}

// Remove the keyboard hook
void HotkeyManager::RemoveHook() {
    if (m_keyboardHook != NULL) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = NULL;
    }
}

// Low-level keyboard hook procedure (static callback)
LRESULT CALLBACK HotkeyManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance != nullptr) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // Get key state (up or down)
        bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (keyDown || keyUp) {
            // Update modifier state
            s_instance->UpdateModifierState(kbd->vkCode, keyDown);

            // If key is down, check for hotkeys
            if (keyDown && s_instance->CheckHotkeys(kbd->vkCode, true)) {
                // Hotkey processed, don't pass to the application
                return 1;
            }
        }
    }

    // Call the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Check if the pressed key combination matches any registered hotkey
bool HotkeyManager::CheckHotkeys(DWORD keyCode, bool isGlobal) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a hotkey with the current modifier state
    Hotkey current(keyCode, m_ctrlDown, m_altDown, m_shiftDown, m_winDown);

    // Check if this matches any registered hotkey
    for (const auto& [name, pair] : m_hotkeyMap) {
        const Hotkey& hotkey = pair.first;

        if (current == hotkey) {
            // Execute the action
            if (pair.second) {
                pair.second();
            }
            return true;
        }
    }

    return false;
}

// Update modifier key state
void HotkeyManager::UpdateModifierState(DWORD keyCode, bool keyDown) {
    switch (keyCode) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        m_ctrlDown = keyDown;
        break;

    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        m_altDown = keyDown;
        break;

    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        m_shiftDown = keyDown;
        break;

    case VK_LWIN:
    case VK_RWIN:
        m_winDown = keyDown;
        break;
    }
}

// Register default hotkeys
void HotkeyManager::RegisterDefaultHotkeys() {
    // Toggle overlay (ESC key)
    RegisterHotkey("toggle_overlay", Hotkey(VK_ESCAPE), [this]() {
        if (m_windowManager) {
            m_windowManager->SetActive(!m_windowManager->IsActive());
        }
        });

    // Switch to main tab (Alt+1)
    RegisterHotkey("show_main", Hotkey('1', false, true), []() {
        // This will be implemented in UISystem integration
        });

    // Switch to browser tab (Alt+2)
    RegisterHotkey("show_browser", Hotkey('2', false, true), []() {
        // This will be implemented in UISystem integration
        });

    // Switch to links tab (Alt+3)
    RegisterHotkey("show_links", Hotkey('3', false, true), []() {
        // This will be implemented in UISystem integration
        });

    // Switch to settings tab (Alt+4)
    RegisterHotkey("show_settings", Hotkey('4', false, true), []() {
        // This will be implemented in UISystem integration
        });

    // Show/hide overlay (Ctrl+Space)
    RegisterHotkey("show_hide", Hotkey(VK_SPACE, true), [this]() {
        if (m_windowManager) {
            m_windowManager->SetVisible(!m_windowManager->IsVisible());
        }
        });
}