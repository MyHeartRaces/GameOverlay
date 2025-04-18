// GameOverlay - main.cpp
// Main application entry point

#include <Windows.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <vector> // Include vector for buffer copy
#include "GameOverlay.h"
#include "PipelineStateManager.h"
#include "CommandAllocatorPool.h"
#include "ResourceManager.h" // Include ResourceManager

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Global references for WindowProc
HotkeyManager* g_hotkeyManager = nullptr; // TODO: Consider better context passing than globals

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        // Create window manager
        auto windowManager = std::make_unique<WindowManager>(hInstance, WindowProc);

        // Create DirectX 12 render system
        auto renderSystem = std::make_unique<RenderSystem>(windowManager->GetHWND(), windowManager->GetWidth(), windowManager->GetHeight());

        // Get Resource Manager (created inside RenderSystem)
        ResourceManager* resourceManager = renderSystem->GetResourceManager();
        if (!resourceManager) {
            throw std::runtime_error("Failed to get Resource Manager from Render System");
        }

        // Create performance monitor
        auto performanceMonitor = std::make_unique<PerformanceMonitor>();

        // Create hotkey manager
        auto hotkeyManager = std::make_unique<HotkeyManager>(windowManager.get());
        g_hotkeyManager = hotkeyManager.get(); // Set global reference

        // Create pipeline state manager for DirectX 12
        auto pipelineStateManager = std::make_unique<PipelineStateManager>(renderSystem.get());
        pipelineStateManager->Initialize(); // Pre-create common states

        // Create command allocator pool (Optional - RenderSystem might manage internally)
        // If RenderSystem handles allocators, this isn't strictly needed here.
        // Assuming RenderSystem needs it or uses a similar pattern:
        auto commandAllocatorPool = std::make_unique<CommandAllocatorPool>(
            renderSystem->GetDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);

        // Create browser view (Needs RenderSystem)
        auto browserView = std::make_unique<BrowserView>(renderSystem.get());
        if (!browserView->Initialize()) {
            // Initialize might return false if it's a CEF subprocess, which isn't an error for the main app.
            // Check manager state if needed, or rely on Initialize throwing for real errors.
            if (browserView->GetBrowserManager() && !browserView->GetBrowserManager()->m_isSubprocess) {
                MessageBoxA(nullptr, "Failed to initialize browser view", "Error", MB_OK | MB_ICONERROR);
                return 1;
            }
        }

        // Create performance optimizer (Needs WindowManager, RenderSystem, BrowserView, PerfMonitor)
        auto performanceOptimizer = std::make_unique<PerformanceOptimizer>(
            windowManager.get(),
            renderSystem.get(),
            browserView.get(),
            performanceMonitor.get());
        performanceOptimizer->Initialize(); // Start optimizer background tasks etc.

        // Load initial URL if manager is initialized (not a subprocess)
        if (browserView->GetBrowserManager() && browserView->GetBrowserManager()->m_initialized) {
            browserView->Navigate("https://www.google.com");
        }

        // Create ImGui system (Needs HWND, RenderSystem)
        auto imguiSystem = std::make_unique<ImGuiSystem>(windowManager->GetHWND(), renderSystem.get());

        // Create UI system (Needs RenderSystem, BrowserView, HotkeyManager, PerfOptimizer, PerfMonitor)
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

            // --- Frame Start ---
            performanceMonitor->BeginFrame();
            performanceOptimizer->UpdateState(); // Determine current performance state

            // --- Browser Update ---
            // Process CEF message loop and check for paint events
            // This might trigger BrowserView::SignalTextureUpdateFromHandler via OnPaint
            if (browserView->GetBrowserManager() && browserView->GetBrowserManager()->m_initialized) {
                browserView->Update();
            }

            // Apply frame throttling based on optimizer state *before* rendering
            performanceOptimizer->ThrottleFrame();

            // --- Render Preparation ---
            renderSystem->BeginFrame(); // Resets command list, sets RT, clears
            ID3D12GraphicsCommandList* commandList = renderSystem->GetCommandList();

            // --- Browser Texture GPU Copy ---
            // Check if the browser signalled a texture update and perform the GPU copy
            if (browserView->TextureNeedsGPUCopy()) {
                // Lock to safely access buffer details potentially written by CEF thread
                std::lock_guard<std::mutex> lock(browserView->m_bufferMutex); // Use the mutex from BrowserView

                const void* cpuBuffer = browserView->GetCpuBufferData();
                int cpuBufferWidth = browserView->GetCpuBufferWidth();
                int cpuBufferHeight = browserView->GetCpuBufferHeight();

                if (cpuBuffer && cpuBufferWidth > 0 && cpuBufferHeight > 0 &&
                    browserView->GetUploadTexture() && browserView->GetTexture())
                {
                    // 1. Copy CPU data (from CEF buffer) to Upload Buffer
                    D3D12_RANGE readRange = { 0, 0 }; // We are writing, not reading
                    void* mappedData = nullptr;
                    HRESULT hr = browserView->GetUploadTexture()->Map(0, &readRange, &mappedData);
                    if (SUCCEEDED(hr)) {
                        size_t srcRowPitch = static_cast<size_t>(cpuBufferWidth) * 4; // BGRA
                        size_t dstRowPitch = (srcRowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
                        size_t dstBufferSize = dstRowPitch * cpuBufferHeight; // Use actual buffer height

                        // Ensure buffer is large enough
                        D3D12_RESOURCE_DESC uploadDesc = browserView->GetUploadTexture()->GetDesc();
                        if (dstBufferSize <= uploadDesc.Width) {
                            // Copy row by row
                            for (int y = 0; y < cpuBufferHeight; ++y) {
                                memcpy(
                                    static_cast<uint8_t*>(mappedData) + y * dstRowPitch,
                                    static_cast<const uint8_t*>(cpuBuffer) + y * srcRowPitch,
                                    srcRowPitch // Copy only the valid data width
                                );
                            }
                            // Range written (can be conservative)
                            D3D12_RANGE writeRange = { 0, dstBufferSize };
                            browserView->GetUploadTexture()->Unmap(0, &writeRange);

                            // 2. Record GPU copy command (Upload Buffer -> Target Texture)
                            resourceManager->TransitionResource(
                                commandList,
                                browserView->GetTexture(), // Target GPU texture
                                D3D12_RESOURCE_STATE_COPY_DEST
                            );

                            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
                            srcLocation.pResource = browserView->GetUploadTexture();
                            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                            srcLocation.PlacedFootprint.Offset = 0;
                            srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                            srcLocation.PlacedFootprint.Footprint.Width = cpuBufferWidth;
                            srcLocation.PlacedFootprint.Footprint.Height = cpuBufferHeight;
                            srcLocation.PlacedFootprint.Footprint.Depth = 1;
                            srcLocation.PlacedFootprint.Footprint.RowPitch = (UINT)dstRowPitch; // Use aligned pitch

                            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
                            dstLocation.pResource = browserView->GetTexture();
                            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                            dstLocation.SubresourceIndex = 0;

                            commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

                            // 3. Transition target texture back for rendering
                            resourceManager->TransitionResource(
                                commandList,
                                browserView->GetTexture(),
                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                            );
                        }
                        else {
                            // Buffer size mismatch - log error
                            OutputDebugStringA("Error: Upload buffer size mismatch during browser texture copy.\n");
                            browserView->GetUploadTexture()->Unmap(0, nullptr); // Unmap even on error
                        }
                    }
                    else {
                        OutputDebugStringA("Error: Failed to map upload buffer for browser texture.\n");
                    }
                }
                browserView->ClearTextureUpdateFlag(); // Clear flag regardless of success/failure
            }

            // --- UI Rendering ---
            imguiSystem->BeginFrame(); // Starts ImGui frame
            uiSystem->Render();        // Renders all UI pages and elements
            imguiSystem->EndFrame();   // Generates ImGui draw data and records render commands

            // --- Frame End ---
            renderSystem->EndFrame(); // Executes command list, presents swap chain
            performanceMonitor->EndFrame(); // Collect metrics
        }

        // --- Cleanup ---
        performanceOptimizer->Suspend(); // Stop optimizer tasks cleanly

        // Wait for GPU before destroying anything that might be in use
        renderSystem->WaitForGpu();

        // Explicitly shutdown browser view before other systems that might reference it
        // (Manager shutdown is handled within BrowserView's destructor)
        if (browserView) browserView->Shutdown(); // Ensure browser is down

        // Destructors handle the rest in reverse order of declaration:
        // uiSystem, imguiSystem, performanceOptimizer, browserView, commandAllocatorPool,
        // pipelineStateManager, hotkeyManager, performanceMonitor, renderSystem, windowManager

        g_hotkeyManager = nullptr; // Clear global reference

        return static_cast<int>(msg.wParam); // Return quit code
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
        return 1; // Return error code
    }
    catch (...) {
        MessageBoxA(nullptr, "An unknown fatal error occurred.", "Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}

// Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Let ImGui handle input first
    if (ImGuiSystem::ProcessMessage(hwnd, uMsg, wParam, lParam))
        return true; // ImGui handled it

    // Retrieve the WindowManager instance (set during WM_CREATE)
    // Using GetWindowLongPtr for 64-bit compatibility
    WindowManager* pWindowManager = reinterpret_cast<WindowManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_CREATE: {
        // Store the 'this' pointer passed from CreateWindowEx
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
        return 0;
    }

    case WM_SIZE: {
        // Only handle resize if RenderSystem is initialized and window manager pointer is valid
        // WM_SIZE can be sent before RenderSystem is ready or after cleanup
        if (pWindowManager && wParam != SIZE_MINIMIZED) {
            // Pass the new client area size
            // Note: RenderSystem::Resize should be called from here or main loop
            // Currently called from main loop is likely okay, but check implications.
            // For now, just update WindowManager's state.
            pWindowManager->HandleResize(LOWORD(lParam), HIWORD(lParam));
            // TODO: Trigger RenderSystem->Resize() here if needed immediately
        }
        return 0;
    }

                // Intercept activation messages to update optimizer state
    case WM_ACTIVATEAPP: {
        // TODO: Signal Active/Inactive state change to PerformanceOptimizer
        // if (pPerformanceOptimizer) {
        //     pPerformanceOptimizer->SetApplicationActive(wParam == TRUE);
        // }
        break; // Still pass to DefWindowProc
    }

                       // Handle standard keyboard input for hotkeys (if hook doesn't catch everything)
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_hotkeyManager && g_hotkeyManager->ProcessKeyEvent(wParam, lParam)) {
            // Optional: return 0 if key was handled *and* shouldn't be processed further
            // return 0;
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (g_hotkeyManager) {
            g_hotkeyManager->ProcessKeyEvent(wParam, lParam); // Process key up for modifier state
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0); // Signal application quit
        return 0;
    }

    // Default message handling
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}