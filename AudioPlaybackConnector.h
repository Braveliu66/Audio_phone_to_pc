#pragma once

#include "resource.h"

using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Audio;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Hosting;
namespace fs = std::filesystem;

constexpr UINT WM_NOTIFYICON = WM_APP + 1;
constexpr UINT WM_CONNECTDEVICE = WM_APP + 2;
constexpr UINT WM_CONNECTION_CLOSED = WM_APP + 3;
constexpr UINT WM_RECOVERDEVICE = WM_APP + 4;
constexpr UINT WM_APPLY_LANGUAGE = WM_APP + 5;

enum class LanguageMode
{
	System = 0,
	English = 1,
	ChineseSimplified = 2
};

HINSTANCE g_hInst;
HWND g_hWnd;
HWND g_hWndXaml;
Canvas g_xamlCanvas = nullptr;
Flyout g_xamlFlyout = nullptr;
MenuFlyout g_xamlMenu = nullptr;
FocusState g_menuFocusState = FocusState::Unfocused;
DevicePicker g_devicePicker = nullptr;
std::unordered_map<std::wstring, std::pair<DeviceInformation, AudioPlaybackConnection>> g_audioPlaybackConnections;
std::unordered_set<std::wstring> g_recoveringDevices;
HICON g_hIconLight = nullptr;
HICON g_hIconDark = nullptr;
NOTIFYICONDATAW g_nid = {
	.cbSize = sizeof(g_nid),
	.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
	.uCallbackMessage = WM_NOTIFYICON,
	.uVersion = NOTIFYICON_VERSION_4
};
NOTIFYICONIDENTIFIER g_niid = {
	.cbSize = sizeof(g_niid)
};
UINT WM_TASKBAR_CREATED = 0;
bool g_reconnect = false;
bool g_isShuttingDown = false;
bool g_preferBestLink = false;
bool g_allowMultiDevice = true;
LanguageMode g_languageMode = LanguageMode::System;
bool g_isMenuOpen = false;
bool g_languageRefreshPending = false;
std::vector<std::wstring> g_lastDevices;
std::unordered_map<std::wstring, uint32_t> g_retryCounts;
std::unordered_set<std::wstring> g_manualDisconnectDevices;

#include "Util.hpp"
#include "I18n.hpp"
#include "SettingsUtil.hpp"
#include "Direct2DSvg.hpp"
