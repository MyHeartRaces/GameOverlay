cmake_minimum_required(VERSION 3.16)
project(GameOverlay VERSION 0.5.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For MSVC, add appropriate compile flags
if(MSVC)
    # Use multi-threaded static runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Disable warning about unsafe functions
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # Add Windows-specific compile definitions
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Find DirectX packages
find_package(directx-headers CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

# Find CEF package
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Add CEF-specific settings
include_directories(${CEF_ROOT})
include_directories(${CEF_ROOT}/include)
add_definitions(-DUSING_CEF_SHARED)

# Add Dear ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# Collect source files
set(SOURCES
    main.cpp
    WindowManager.cpp
    RenderSystem.cpp
    ImGuiSystem.cpp
    PerformanceMonitor.cpp
    BrowserManager.cpp
    BrowserApp.cpp
    BrowserClient.cpp
    BrowserHandler.cpp
    BrowserView.cpp
    UISystem.cpp
    PageBase.cpp
    MainPage.cpp
    BrowserPage.cpp
    LinksPage.cpp
    SettingsPage.cpp
    HotkeyManager.cpp
    HotkeySettingsPage.cpp
    PerformanceOptimizer.cpp
    ResourceManager.cpp
    PerformanceSettingsPage.cpp
)

# Collect header files
set(HEADERS
    GameOverlay.h
    WindowManager.h
    RenderSystem.h
    ImGuiSystem.h
    PerformanceMonitor.h
    BrowserManager.h
    BrowserApp.h
    BrowserClient.h
    BrowserHandler.h
    BrowserView.h
    UISystem.h
    PageBase.h
    MainPage.h
    BrowserPage.h
    LinksPage.h
    SettingsPage.h
    HotkeyManager.h
    HotkeySettingsPage.h
    PerformanceOptimizer.h
    ResourceManager.h
    PerformanceSettingsPage.h
)

# Create executable
add_executable(GameOverlay WIN32 ${SOURCES} ${HEADERS} ${IMGUI_SOURCES} ${CEF_RESOURCES})

# Add include directories
target_include_directories(GameOverlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CEF_ROOT}
)

# Link libraries
target_link_libraries(GameOverlay PRIVATE
    d3d11.lib
    dxgi.lib
    Microsoft::DirectX-Headers
    Microsoft::DirectXTK
    ${CEF_LIBRARIES}
)

# Add CEF binary distribution to output directory
add_custom_command(
    TARGET GameOverlay
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_BINARY_DIR}/Release" "$<TARGET_FILE_DIR:GameOverlay>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:GameOverlay>"
)

# Install target
install(TARGETS GameOverlay DESTINATION bin)

# Set VS startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GameOverlay)

# Output some information
message(STATUS "GameOverlay - Phase 5: Performance Optimization")
message(STATUS "Using C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CEF Root: ${CEF_ROOT}")