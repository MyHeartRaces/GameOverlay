// GameOverlay - main.cpp
// Phase 6: DirectX 12 Migration
// Main application entry point

#include <Windows.h>
#include <string>
#include <memory>
#include <stdexcept>
#include "GameOverlay.h"
#include "PipelineStateManager.h"
#include "CommandAllocatorPool.h"

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Global references for WindowProc
HotkeyManager* g_hotkeyManager = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        // Create window manager
        auto windowManager = std::make_unique<WindowManager>(hInstance, WindowProc);

        // Create DirectX 12 render system
        auto renderSystem = std::make_unique<RenderSystem>(windowManager->GetHWND(), windowManager->GetWidth(), windowManager->GetHeight());

        // Create performance monitor
        auto performanceMonitor = std::make_unique<PerformanceMonitor>();

        // Create hotkey manager
        auto hotkeyManager = std::make_unique<HotkeyManager>(windowManager.get());

        // Set global reference for WindowProc
        g_hotkeyManager = hotkeyManager.get();

        // Create pipeline state manager for DirectX 12
        auto pipelineStateManager = std::make_unique<PipelineStateManager>(renderSystem.get());
        pipelineStateManager->Initialize();

        // Create command allocator pool for DirectX 12
        auto commandAllocatorPool = std::make_unique<CommandAllocatorPool>(
            renderSystem->GetDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);

        // Create browser view
        auto browserView = std::make_unique<BrowserView>(renderSystem.get());
        if (!browserView->Initialize()) {
            MessageBoxA(nullptr, "Failed to initialize browser view", "Error", MB_OK | MB_ICONERROR);
            return 1;
        }

        // Create performance optimizer
        auto performanceOptimizer = std::make_unique<PerformanceOptimizer>(
            windowManager.get(),
            renderSystem.get(),
            browserView.get(),
            performanceMonitor.get());

        // Initialize performance optimizer
        performanceOptimizer->Initialize();

        // Load initial URL
        browserView->Navigate("https://www.google.com");

        // Create ImGui system
        auto imguiSystem = std::make_unique<ImGuiSystem>(windowManager->GetHWND(), renderSystem.get());

        // Create UI system with all components
        auto uiSystem = std::make_unique<UISystem>(
            renderSystem.get(),
            browserView.get(),
            hotkeyManager.get(),
            performanceOptimizer.get(),
            performanceMonitor.get());

        // Main message loop
        MSG msg = {};
        bool running = true;

        while (running) {
            // Process Windows messages
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
            }

            if (!running) break;

            // Begin frame
            performanceMonitor->BeginFrame();

            // Update performance state
            performanceOptimizer->UpdateState();

            // Process browser with performance considerations
            browserView->Render();

            // Apply frame throttling if needed
            performanceOptimizer->ThrottleFrame();

            // Begin render
            renderSystem->BeginFrame();

            // Begin ImGui frame
            imguiSystem->BeginFrame();

            // Render UI system (includes browser UI)
            uiSystem->Render();

            // End ImGui frame
            imguiSystem->EndFrame();

            // End render
            renderSystem->EndFrame();

            // End frame and collect metrics
            performanceMonitor->EndFrame();
        }

        // Cleanup in reverse order of creation
        performanceOptimizer->Suspend();

        // Make sure GPU is done before destroying resources
        renderSystem->WaitForGpu();

        // Explicitly shut down browser before other resources
        browserView->Shutdown();

        // Cleanup is handled by destructors of the unique_ptr objects
        return static_cast<int>(msg.wParam);
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Process ImGui input messages
    if (ImGuiSystem::ProcessMessage(hwnd, uMsg, wParam, lParam))
        return true;

    // Get pointer to window instance
    WindowManager* windowManager = reinterpret_cast<WindowManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_CREATE: {
        // Store pointer to window manager in window's user data
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        // Handle window resize
        if (windowManager) {
            windowManager->HandleResize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        // Process key events through hotkey manager
        if (g_hotkeyManager && g_hotkeyManager->ProcessKeyEvent(wParam, lParam)) {
            return 0; // Key was handled by hotkey manager
        }
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}