// GameOverlay - GameOverlay.h
// Phase 6: DirectX 12 Migration
// Main include file for the GameOverlay application

#pragma once

#include "WindowManager.h"
#include "RenderSystem.h"
#include "ImGuiSystem.h"
#include "PerformanceMonitor.h"
#include "BrowserManager.h"
#include "BrowserView.h"
#include "UISystem.h"
#include "PageBase.h"
#include "MainPage.h"
#include "BrowserPage.h"
#include "LinksPage.h"
#include "SettingsPage.h"
#include "HotkeyManager.h"
#include "HotkeySettingsPage.h"
#include "PerformanceOptimizer.h"
#include "ResourceManager.h"
#include "PerformanceSettingsPage.h"
#include "PipelineStateManager.h"
#include "CommandAllocatorPool.h"

// Version information
#define GAMEOVERLAY_VERSION_MAJOR 0
#define GAMEOVERLAY_VERSION_MINOR 6
#define GAMEOVERLAY_VERSION_PATCH 0
#define GAMEOVERLAY_VERSION_STRING "0.6.0"
#define GAMEOVERLAY_PHASE "DirectX 12 Migration"

// Global variables
extern HotkeyManager* g_hotkeyManager;