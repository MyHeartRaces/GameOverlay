cmake_minimum_required(VERSION 3.16)
project(GameOverlay VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find DirectX 12 packages
find_package(DirectX REQUIRED)

# Find CEF
if(NOT DEFINED CEF_ROOT)
    message(FATAL_ERROR "CEF_ROOT must be specified!")
endif()

set(CEF_INCLUDE_DIRS
    ${CEF_ROOT}
    ${CEF_ROOT}/include
)

# Set CEF library paths based on build type
set(CEF_LIBRARIES
    debug ${CEF_ROOT}/Debug/libcef.lib
    optimized ${CEF_ROOT}/Release/libcef.lib
)

# CEF wrapper library
set(CEF_WRAPPER_LIBRARY
    debug ${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib
    optimized ${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib
)

message(STATUS "Using CEF Include Dirs: ${CEF_INCLUDE_DIRS}")
message(STATUS "Using CEF Libraries: ${CEF_LIBRARIES}")
message(STATUS "Using CEF Wrapper Lib: ${CEF_WRAPPER_LIBRARY}")

# Add ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
)

# Source files for the application
set(GAMEOVERLAY_SOURCES
    src/main.cpp
    src/WindowManager.cpp
    src/PerformanceMonitor.cpp
    src/RenderSystem.cpp
    src/ImGuiSystem.cpp
    src/BrowserManager.cpp
    src/BrowserHandler.cpp
    src/BrowserClient.cpp
    src/BrowserApp.cpp
    src/BrowserView.cpp
    src/UISystem.cpp
    src/PageBase.cpp
    src/MainPage.cpp
    src/BrowserPage.cpp
    src/LinksPage.cpp
    src/SettingsPage.cpp
    src/HotkeyManager.cpp
    src/HotkeySettingsPage.cpp
    src/PerformanceOptimizer.cpp
    src/PerformanceSettingsPage.cpp
    src/PipelineStateManager.cpp
    src/CommandAllocatorPool.cpp
    src/ResourceManager.cpp
)

# Header files
set(GAMEOVERLAY_HEADERS
    include/GameOverlay.h
    include/WindowManager.h
    include/PerformanceMonitor.h
    include/RenderSystem.h
    include/ImGuiSystem.h
    include/BrowserManager.h
    include/BrowserHandler.h
    include/BrowserClient.h
    include/BrowserApp.h
    include/BrowserView.h
    include/UISystem.h
    include/PageBase.h
    include/MainPage.h
    include/BrowserPage.h
    include/LinksPage.h
    include/SettingsPage.h
    include/HotkeyManager.h
    include/HotkeySettingsPage.h
    include/PerformanceOptimizer.h
    include/PerformanceSettingsPage.h
    include/PipelineStateManager.h
    include/CommandAllocatorPool.h
    include/ResourceManager.h
)

# Add executable
add_executable(GameOverlay WIN32 
    ${GAMEOVERLAY_SOURCES} 
    ${GAMEOVERLAY_HEADERS}
    ${IMGUI_SOURCES}
)

# Include directories
target_include_directories(GameOverlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CEF_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(GameOverlay PRIVATE
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    ${CEF_LIBRARIES}
    ${CEF_WRAPPER_LIBRARY}
)

# Define preprocessor macros
target_compile_definitions(GameOverlay PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Copy CEF resources to output directory
add_custom_command(TARGET GameOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CEF_ROOT}/Release $<TARGET_FILE_DIR:GameOverlay>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CEF_ROOT}/Resources $<TARGET_FILE_DIR:GameOverlay>
)

# Set startup project (Visual Studio only)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GameOverlay)