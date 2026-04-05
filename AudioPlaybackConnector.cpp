#include "pch.h"
#include "AudioPlaybackConnector.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SetupFlyout();
void SetupMenu();
winrt::fire_and_forget ConnectDevice(DevicePicker, std::wstring_view);
winrt::fire_and_forget ConnectDevice(DevicePicker, DeviceInformation);
winrt::fire_and_forget RecoverDevice(std::wstring);
winrt::fire_and_forget RetryConnectDevice(DeviceInformation, uint32_t);
void SetupDevicePicker();
void SetupSvgIcon();
void UpdateNotifyIcon();
bool IsStartupEnabled();
bool SetStartupEnabled(bool enabled);
[[noreturn]] void ForceShutdown();
void ApplyThreadLanguage();
void RefreshLocalizedUi();
void RefreshConnectionStatuses();
void RequestLanguageRefresh();
void SwitchLanguage(LanguageMode mode);
bool QueueReconnect(DeviceInformation const& device, std::wstring_view fallbackError = {});
void MarkManualDisconnect(std::wstring_view deviceId);
const wchar_t* LinkConnectingStatusText();
const wchar_t* LinkConnectedStatusText();
const wchar_t* LinkRecoveringStatusText();
const wchar_t* LinkRetryingStatusText(bool repeatedAttempt = false);
const wchar_t* LinkSwitchedStatusText();
const wchar_t* RetryStoppedText();

namespace
{
	constexpr auto STARTUP_REG_PATH = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
	constexpr auto STARTUP_VALUE_NAME = L"Audio_phone_to_pc";
	constexpr uint32_t MAX_AUTO_RETRY_ATTEMPTS = 3;
}

bool IsStartupEnabled()
{
	wchar_t exePath[MAX_PATH] = {};
	DWORD exePathCch = ARRAYSIZE(exePath);
	if (GetModuleFileNameW(g_hInst, exePath, exePathCch) == 0)
	{
		LOG_LAST_ERROR();
		return false;
	}

	std::wstring command = L"\"";
	command += exePath;
	command += L"\"";

	std::wstring value(1024, L'\0');
	DWORD cbValue = static_cast<DWORD>(value.size() * sizeof(wchar_t));
	auto ret = RegGetValueW(HKEY_CURRENT_USER, STARTUP_REG_PATH, STARTUP_VALUE_NAME, RRF_RT_REG_SZ, nullptr, value.data(), &cbValue);
	if (ret == ERROR_FILE_NOT_FOUND)
	{
		return false;
	}
	if (ret != ERROR_SUCCESS)
	{
		LOG_IF_WIN32_ERROR(ret);
		return false;
	}

	value.resize(cbValue / sizeof(wchar_t));
	while (!value.empty() && value.back() == L'\0')
	{
		value.pop_back();
	}

	return CompareStringOrdinal(value.c_str(), -1, command.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool SetStartupEnabled(bool enabled)
{
	wil::unique_hkey hKey;
	auto ret = RegCreateKeyExW(HKEY_CURRENT_USER, STARTUP_REG_PATH, 0, nullptr, 0, KEY_SET_VALUE, nullptr, hKey.addressof(), nullptr);
	if (ret != ERROR_SUCCESS)
	{
		LOG_IF_WIN32_ERROR(ret);
		return false;
	}

	if (!enabled)
	{
		ret = RegDeleteValueW(hKey.get(), STARTUP_VALUE_NAME);
		if (ret == ERROR_SUCCESS || ret == ERROR_FILE_NOT_FOUND)
		{
			return true;
		}
		LOG_IF_WIN32_ERROR(ret);
		return false;
	}

	wchar_t exePath[MAX_PATH] = {};
	DWORD exePathCch = ARRAYSIZE(exePath);
	if (GetModuleFileNameW(g_hInst, exePath, exePathCch) == 0)
	{
		LOG_LAST_ERROR();
		return false;
	}

	std::wstring command = L"\"";
	command += exePath;
	command += L"\"";

	ret = RegSetValueExW(hKey.get(), STARTUP_VALUE_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
		static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
	if (ret != ERROR_SUCCESS)
	{
		LOG_IF_WIN32_ERROR(ret);
		return false;
	}
	return true;
}

const wchar_t* LinkConnectingStatusText()
{
	return _(L"Connecting audio...");
}

const wchar_t* LinkConnectedStatusText()
{
	return _(L"Audio ready");
}

const wchar_t* LinkRecoveringStatusText()
{
	return _(L"Reconnecting audio...");
}

const wchar_t* LinkRetryingStatusText(bool repeatedAttempt)
{
	return repeatedAttempt ? _(L"Still trying to reconnect...") : _(L"Connection lost. Retrying automatically...");
}

const wchar_t* LinkSwitchedStatusText()
{
	return _(L"Switched to another device");
}

const wchar_t* RetryStoppedText()
{
	return _(L"Automatic retry stopped. Click to try again.");
}

[[noreturn]] void ForceShutdown()
{
	if (g_isShuttingDown)
	{
		ExitProcess(0);
	}

	g_isShuttingDown = true;
	g_languageRefreshPending = false;
	g_isMenuOpen = false;
	g_recoveringDevices.clear();
	g_retryCounts.clear();
	g_manualDisconnectDevices.clear();

	for (const auto& connection : g_audioPlaybackConnections)
	{
		connection.second.second.Close();
		if (g_devicePicker)
		{
			g_devicePicker.SetDisplayStatus(connection.second.first, {}, DevicePickerDisplayStatusOptions::None);
		}
	}

	SaveSettings();
	g_audioPlaybackConnections.clear();

	if (g_xamlFlyout)
	{
		g_xamlFlyout.Hide();
	}
	if (g_xamlMenu)
	{
		g_xamlMenu.Hide();
	}

	Shell_NotifyIconW(NIM_DELETE, &g_nid);

	if (g_hWndXaml)
	{
		DestroyWindow(g_hWndXaml);
		g_hWndXaml = nullptr;
	}
	if (g_hWnd)
	{
		DestroyWindow(g_hWnd);
		g_hWnd = nullptr;
	}

	g_devicePicker = nullptr;
	g_xamlFlyout = nullptr;
	g_xamlMenu = nullptr;
	g_xamlCanvas = nullptr;

	if (g_hIconLight)
	{
		DestroyIcon(g_hIconLight);
		g_hIconLight = nullptr;
	}
	if (g_hIconDark)
	{
		DestroyIcon(g_hIconDark);
		g_hIconDark = nullptr;
	}

	winrt::uninit_apartment();
	ExitProcess(0);
}

void ApplyThreadLanguage()
{
	LANGID languageId = GetUserDefaultUILanguage();
	switch (g_languageMode)
	{
	case LanguageMode::English:
		languageId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
		break;
	case LanguageMode::ChineseSimplified:
		languageId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
		break;
	case LanguageMode::System:
	default:
		break;
	}

	SetThreadUILanguage(languageId);
}

void RefreshConnectionStatuses()
{
	for (const auto& connection : g_audioPlaybackConnections)
	{
		g_devicePicker.SetDisplayStatus(connection.second.first, LinkConnectedStatusText(), DevicePickerDisplayStatusOptions::ShowDisconnectButton);
	}
}

void RefreshLocalizedUi()
{
	ApplyThreadLanguage();
	SetupFlyout();
	SetupMenu();
	SetupDevicePicker();
	wcscpy_s(g_nid.szTip, _(L"Audio_phone_to_pc"));
	UpdateNotifyIcon();
	RefreshConnectionStatuses();
}

void SwitchLanguage(LanguageMode mode)
{
	if (g_languageMode == mode)
	{
		return;
	}

	g_languageMode = mode;
	SaveSettings();
	RequestLanguageRefresh();
}

void RequestLanguageRefresh()
{
	if (g_isMenuOpen)
	{
		g_languageRefreshPending = true;
		return;
	}

	RefreshLocalizedUi();
}

void MarkManualDisconnect(std::wstring_view deviceId)
{
	std::wstring id(deviceId);
	g_manualDisconnectDevices.insert(id);
	g_retryCounts.erase(id);
	g_recoveringDevices.erase(id);
}

bool QueueReconnect(DeviceInformation const& device, std::wstring_view fallbackError)
{
	auto deviceId = std::wstring(device.Id());
	if (g_isShuttingDown)
	{
		return false;
	}

	if (g_manualDisconnectDevices.erase(deviceId) > 0)
	{
		return false;
	}

	auto& retryCount = g_retryCounts[deviceId];
	if (retryCount >= MAX_AUTO_RETRY_ATTEMPTS)
	{
		g_retryCounts.erase(deviceId);
		if (!fallbackError.empty())
		{
			std::wstring message(fallbackError);
			message += L"\n";
			message += RetryStoppedText();
			g_devicePicker.SetDisplayStatus(device, message, DevicePickerDisplayStatusOptions::ShowRetryButton);
		}
		else
		{
			g_devicePicker.SetDisplayStatus(device, RetryStoppedText(), DevicePickerDisplayStatusOptions::ShowRetryButton);
		}
		return true;
	}

	++retryCount;
	g_devicePicker.SetDisplayStatus(device, LinkRetryingStatusText(retryCount > 1), DevicePickerDisplayStatusOptions::ShowProgress | DevicePickerDisplayStatusOptions::ShowDisconnectButton);
	RetryConnectDevice(device, retryCount);
	return true;
}

winrt::fire_and_forget RetryConnectDevice(DeviceInformation device, uint32_t attempt)
{
	auto delay = std::chrono::milliseconds(800 + (attempt - 1) * 1200);
	co_await winrt::resume_after(delay);

	auto deviceId = std::wstring(device.Id());
	auto it = g_retryCounts.find(deviceId);
	if (g_isShuttingDown || it == g_retryCounts.end() || it->second != attempt)
	{
		co_return;
	}

	ConnectDevice(g_devicePicker, device);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	g_hInst = hInstance;
	LoadTranslateData();

	winrt::init_apartment(winrt::apartment_type::single_threaded);
	LoadSettings();
	ApplyThreadLanguage();

	bool supported = false;
	try
	{
		using namespace winrt::Windows::Foundation::Metadata;

		supported = ApiInformation::IsTypePresent(winrt::name_of<DesktopWindowXamlSource>()) &&
			ApiInformation::IsTypePresent(winrt::name_of<AudioPlaybackConnection>());
	}
	catch (winrt::hresult_error const&)
	{
		supported = false;
		LOG_CAUGHT_EXCEPTION();
	}
	if (!supported)
	{
		TaskDialog(nullptr, nullptr, _(L"Unsupported Operating System"), nullptr, _(L"Audio_phone_to_pc is not supported on this operating system version."), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
		return EXIT_FAILURE;
	}

	WNDCLASSEXW wcex = {
		.cbSize = sizeof(wcex),
		.lpfnWndProc = WndProc,
		.hInstance = hInstance,
		.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_AUDIOPLAYBACKCONNECTOR)),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = L"Audio_phone_to_pc",
		.hIconSm = wcex.hIcon
	};

	RegisterClassExW(&wcex);

	// When parent window size is 0x0 or invisible, the dpi scale of menu is incorrect. Here we set window size to 1x1 and use WS_EX_LAYERED to make window looks like invisible.
	g_hWnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOPMOST, L"Audio_phone_to_pc", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
	FAIL_FAST_LAST_ERROR_IF_NULL(g_hWnd);
	FAIL_FAST_IF_WIN32_BOOL_FALSE(SetLayeredWindowAttributes(g_hWnd, 0, 0, LWA_ALPHA));

	DesktopWindowXamlSource desktopSource;
	auto desktopSourceNative2 = desktopSource.as<IDesktopWindowXamlSourceNative2>();
	winrt::check_hresult(desktopSourceNative2->AttachToWindow(g_hWnd));
	winrt::check_hresult(desktopSourceNative2->get_WindowHandle(&g_hWndXaml));

	g_xamlCanvas = Canvas();
	desktopSource.Content(g_xamlCanvas);

	SetupFlyout();
	SetupMenu();
	SetupDevicePicker();
	SetupSvgIcon();

	g_nid.hWnd = g_niid.hWnd = g_hWnd;
	wcscpy_s(g_nid.szTip, _(L"Audio_phone_to_pc"));
	UpdateNotifyIcon();

	WM_TASKBAR_CREATED = RegisterWindowMessageW(L"TaskbarCreated");
	LOG_LAST_ERROR_IF(WM_TASKBAR_CREATED == 0);

	PostMessageW(g_hWnd, WM_CONNECTDEVICE, 0, 0);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		BOOL processed = FALSE;
		winrt::check_hresult(desktopSourceNative2->PreTranslateMessage(&msg, &processed));
		if (!processed)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		ForceShutdown();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_RECOVERDEVICE:
	{
		std::unique_ptr<std::wstring> deviceId(reinterpret_cast<std::wstring*>(lParam));
		if (deviceId && !g_isShuttingDown)
		{
			RecoverDevice(std::move(*deviceId));
		}
	}
	break;
	case WM_CONNECTION_CLOSED:
	{
		std::unique_ptr<std::wstring> deviceId(reinterpret_cast<std::wstring*>(lParam));
		if (!deviceId || g_isShuttingDown)
		{
			break;
		}

		auto it = g_audioPlaybackConnections.find(*deviceId);
		if (it != g_audioPlaybackConnections.end())
		{
			auto device = it->second.first;
			g_audioPlaybackConnections.erase(it);
			QueueReconnect(device);
		}
	}
	break;
	case WM_SETTINGCHANGE:
		if (lParam && CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam), -1, L"ImmersiveColorSet", -1, TRUE) == CSTR_EQUAL)
		{
			UpdateNotifyIcon();
		}
		break;
	case WM_APPLY_LANGUAGE:
		g_languageRefreshPending = false;
		RefreshLocalizedUi();
		break;
	case WM_NOTIFYICON:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
		case NIN_KEYSELECT:
		{
			using namespace winrt::Windows::UI::Popups;

			RECT iconRect;
			auto hr = Shell_NotifyIconGetRect(&g_niid, &iconRect);
			if (FAILED(hr))
			{
				LOG_HR(hr);
				break;
			}

			auto dpi = GetDpiForWindow(hWnd);
			Rect rect = {
				static_cast<float>(iconRect.left * USER_DEFAULT_SCREEN_DPI / dpi),
				static_cast<float>(iconRect.top * USER_DEFAULT_SCREEN_DPI / dpi),
				static_cast<float>((iconRect.right - iconRect.left) * USER_DEFAULT_SCREEN_DPI / dpi),
				static_cast<float>((iconRect.bottom - iconRect.top) * USER_DEFAULT_SCREEN_DPI / dpi)
			};

			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_HIDEWINDOW);
			SetForegroundWindow(hWnd);
			g_devicePicker.Show(rect, Placement::Above);
		}
		break;
		case WM_RBUTTONUP: // Menu activated by mouse click
			g_menuFocusState = FocusState::Pointer;
			break;
		case WM_CONTEXTMENU:
		{
			if (g_menuFocusState == FocusState::Unfocused)
				g_menuFocusState = FocusState::Keyboard;

			auto dpi = GetDpiForWindow(hWnd);
			Point point = {
				static_cast<float>(GET_X_LPARAM(wParam) * USER_DEFAULT_SCREEN_DPI / dpi),
				static_cast<float>(GET_Y_LPARAM(wParam) * USER_DEFAULT_SCREEN_DPI / dpi)
			};

			SetWindowPos(g_hWndXaml, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_SHOWWINDOW);
			SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 1, 1, SWP_SHOWWINDOW);
			SetForegroundWindow(hWnd);

			g_xamlMenu.ShowAt(g_xamlCanvas, point);
		}
		break;
		}
		break;
	case WM_CONNECTDEVICE:
		if (g_reconnect)
		{
			for (const auto& i : g_lastDevices)
			{
				ConnectDevice(g_devicePicker, i);
			}
			g_lastDevices.clear();
		}
		break;
	default:
		if (WM_TASKBAR_CREATED && message == WM_TASKBAR_CREATED)
		{
			UpdateNotifyIcon();
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

void SetupFlyout()
{
	TextBlock textBlock;
	textBlock.Text(_(L"All connections will be closed.\nExit anyway?"));
	textBlock.Margin({ 0, 0, 0, 12 });

	CheckBox checkbox;
	checkbox.IsChecked(g_reconnect);
	checkbox.Content(winrt::box_value(_(L"Reconnect on next start")));

	Button button;
	button.Content(winrt::box_value(_(L"Exit")));
	button.HorizontalAlignment(HorizontalAlignment::Right);
	button.Click([checkbox](const auto&, const auto&) {
		g_reconnect = checkbox.IsChecked().Value();
		PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
	});

	StackPanel stackPanel;
	stackPanel.Children().Append(textBlock);
	stackPanel.Children().Append(checkbox);
	stackPanel.Children().Append(button);

	Flyout flyout;
	flyout.ShouldConstrainToRootBounds(false);
	flyout.Content(stackPanel);

	g_xamlFlyout = flyout;
}

void SetupMenu()
{
	// https://docs.microsoft.com/en-us/windows/uwp/design/style/segoe-ui-symbol-font
	FontIcon startupIcon;
	startupIcon.Glyph(L"\xE7B7");

	ToggleMenuFlyoutItem startupItem;
	startupItem.Text(_(L"Run at startup"));
	startupItem.Icon(startupIcon);
	startupItem.IsChecked(IsStartupEnabled());
	startupItem.Click([](const auto& sender, const auto&) {
		auto item = sender.as<ToggleMenuFlyoutItem>();
		auto checked = item.IsChecked();
		if (!SetStartupEnabled(checked))
		{
			item.IsChecked(!checked);
		}
	});

	FontIcon multiDeviceIcon;
	multiDeviceIcon.Glyph(L"\xE7B8");

	ToggleMenuFlyoutItem multiDeviceItem;
	multiDeviceItem.Text(_(L"Allow multiple connected devices"));
	multiDeviceItem.Icon(multiDeviceIcon);
	multiDeviceItem.IsChecked(g_allowMultiDevice);
	multiDeviceItem.Click([](const auto& sender, const auto&) {
		auto item = sender.as<ToggleMenuFlyoutItem>();
		g_allowMultiDevice = item.IsChecked();
		g_preferBestLink = !g_allowMultiDevice;
		SaveSettings();
	});

	FontIcon settingsIcon;
	settingsIcon.Glyph(L"\xE713");

	MenuFlyoutItem settingsItem;
	settingsItem.Text(_(L"Bluetooth Settings"));
	settingsItem.Icon(settingsIcon);
	settingsItem.Click([](const auto&, const auto&) {
		winrt::Windows::System::Launcher::LaunchUriAsync(Uri(L"ms-settings:bluetooth"));
	});

	FontIcon deviceSettingsIcon;
	deviceSettingsIcon.Glyph(L"\xE8B8");

	MenuFlyoutItem deviceSettingsItem;
	deviceSettingsItem.Text(_(L"Device Settings"));
	deviceSettingsItem.Icon(deviceSettingsIcon);
	deviceSettingsItem.Click([](const auto&, const auto&) {
		winrt::Windows::System::Launcher::LaunchUriAsync(Uri(L"ms-settings:devices"));
	});

	FontIcon soundSettingsIcon;
	soundSettingsIcon.Glyph(L"\xE15D");

	MenuFlyoutItem soundSettingsItem;
	soundSettingsItem.Text(_(L"Sound Settings"));
	soundSettingsItem.Icon(soundSettingsIcon);
	soundSettingsItem.Click([](const auto&, const auto&) {
		winrt::Windows::System::Launcher::LaunchUriAsync(Uri(L"ms-settings:sound"));
	});

	FontIcon appVolumeIcon;
	appVolumeIcon.Glyph(L"\xE9E9");

	MenuFlyoutItem appVolumeItem;
	appVolumeItem.Text(_(L"App Volume Settings"));
	appVolumeItem.Icon(appVolumeIcon);
	appVolumeItem.Click([](const auto&, const auto&) {
		winrt::Windows::System::Launcher::LaunchUriAsync(Uri(L"ms-settings:apps-volume"));
	});

	FontIcon linkInfoIcon;
	linkInfoIcon.Glyph(L"\xE946");

	MenuFlyoutItem linkInfoItem;
	linkInfoItem.Text(_(L"Connection Tips"));
	linkInfoItem.Icon(linkInfoIcon);
	linkInfoItem.Click([](const auto&, const auto&) {
		TaskDialog(g_hWnd, nullptr, _(L"Connection Tips"), nullptr,
			_(L"Switching devices will disconnect the previous audio device to keep playback stable.\n\nIf a phone or tablet wakes up slowly, the app will retry automatically a few times for you."),
			TDCBF_OK_BUTTON, TD_INFORMATION_ICON, nullptr);
	});

	MenuFlyoutSubItem languageItem;
	languageItem.Text(_(L"Language"));

	ToggleMenuFlyoutItem followSystemLanguageItem;
	followSystemLanguageItem.Text(_(L"Follow system language"));
	followSystemLanguageItem.IsChecked(g_languageMode == LanguageMode::System);
	followSystemLanguageItem.Click([](const auto&, const auto&) {
		SwitchLanguage(LanguageMode::System);
	});

	ToggleMenuFlyoutItem englishLanguageItem;
	englishLanguageItem.Text(_(L"English"));
	englishLanguageItem.IsChecked(g_languageMode == LanguageMode::English);
	englishLanguageItem.Click([](const auto&, const auto&) {
		SwitchLanguage(LanguageMode::English);
	});

	ToggleMenuFlyoutItem chineseLanguageItem;
	chineseLanguageItem.Text(_(L"Simplified Chinese"));
	chineseLanguageItem.IsChecked(g_languageMode == LanguageMode::ChineseSimplified);
	chineseLanguageItem.Click([](const auto&, const auto&) {
		SwitchLanguage(LanguageMode::ChineseSimplified);
	});

	languageItem.Items().Append(followSystemLanguageItem);
	languageItem.Items().Append(englishLanguageItem);
	languageItem.Items().Append(chineseLanguageItem);

	FontIcon closeIcon;
	closeIcon.Glyph(L"\xE8BB");

	MenuFlyoutItem exitItem;
	exitItem.Text(_(L"Exit"));
	exitItem.Icon(closeIcon);
	exitItem.Click([](const auto&, const auto&) {
		if (g_audioPlaybackConnections.size() == 0)
		{
			PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
			return;
		}

		RECT iconRect;
		auto hr = Shell_NotifyIconGetRect(&g_niid, &iconRect);
		if (FAILED(hr))
		{
			LOG_HR(hr);
			return;
		}

		auto dpi = GetDpiForWindow(g_hWnd);

		SetWindowPos(g_hWnd, HWND_TOPMOST, iconRect.left, iconRect.top, 0, 0, SWP_HIDEWINDOW);
		g_xamlCanvas.Width(static_cast<float>((iconRect.right - iconRect.left) * USER_DEFAULT_SCREEN_DPI / dpi));
		g_xamlCanvas.Height(static_cast<float>((iconRect.bottom - iconRect.top) * USER_DEFAULT_SCREEN_DPI / dpi));

		g_xamlFlyout.ShowAt(g_xamlCanvas);
	});

	MenuFlyout menu;
	menu.Items().Append(startupItem);
	menu.Items().Append(multiDeviceItem);
	menu.Items().Append(deviceSettingsItem);
	menu.Items().Append(settingsItem);
	menu.Items().Append(soundSettingsItem);
	menu.Items().Append(appVolumeItem);
	menu.Items().Append(linkInfoItem);
	menu.Items().Append(languageItem);
	menu.Items().Append(exitItem);
	menu.Opened([](const auto& sender, const auto&) {
		g_isMenuOpen = true;
		auto menuItems = sender.as<MenuFlyout>().Items();
		if (menuItems.Size() > 0)
		{
			auto startup = menuItems.GetAt(0).try_as<ToggleMenuFlyoutItem>();
			if (startup)
			{
				startup.IsChecked(IsStartupEnabled());
			}
		}
		if (menuItems.Size() > 1)
		{
			auto multiDevice = menuItems.GetAt(1).try_as<ToggleMenuFlyoutItem>();
			if (multiDevice)
			{
				multiDevice.IsChecked(g_allowMultiDevice);
			}
		}

		auto itemsCount = menuItems.Size();
		if (itemsCount > 0)
		{
			menuItems.GetAt(itemsCount - 1).Focus(g_menuFocusState);
		}
		g_menuFocusState = FocusState::Unfocused;
	});
	menu.Closed([](const auto&, const auto&) {
		g_isMenuOpen = false;
		ShowWindow(g_hWnd, SW_HIDE);
		if (g_languageRefreshPending)
		{
			PostMessageW(g_hWnd, WM_APPLY_LANGUAGE, 0, 0);
		}
	});

	g_xamlMenu = menu;
}

winrt::fire_and_forget ConnectDevice(DevicePicker picker, DeviceInformation device)
{
	auto deviceId = std::wstring(device.Id());
	g_manualDisconnectDevices.erase(deviceId);
	picker.SetDisplayStatus(device, LinkConnectingStatusText(), DevicePickerDisplayStatusOptions::ShowProgress | DevicePickerDisplayStatusOptions::ShowDisconnectButton);

	bool success = false;
	std::wstring errorMessage;

	try
	{
		if (g_audioPlaybackConnections.find(deviceId) != g_audioPlaybackConnections.end())
		{
			g_retryCounts.erase(deviceId);
			picker.SetDisplayStatus(device, LinkConnectedStatusText(), DevicePickerDisplayStatusOptions::ShowDisconnectButton);
			co_return;
		}

		auto connection = AudioPlaybackConnection::TryCreateFromId(device.Id());
		if (connection)
		{
			if (!g_allowMultiDevice)
			{
				std::vector<std::wstring> otherDeviceIds;
				otherDeviceIds.reserve(g_audioPlaybackConnections.size());
				for (const auto& kv : g_audioPlaybackConnections)
				{
					if (kv.first != deviceId)
					{
						otherDeviceIds.push_back(kv.first);
					}
				}
				for (const auto& id : otherDeviceIds)
				{
					auto it = g_audioPlaybackConnections.find(id);
					if (it != g_audioPlaybackConnections.end())
					{
						MarkManualDisconnect(id);
						it->second.second.Close();
						g_devicePicker.SetDisplayStatus(it->second.first, LinkSwitchedStatusText(), DevicePickerDisplayStatusOptions::ShowRetryButton);
						g_audioPlaybackConnections.erase(it);
					}
				}
			}

			g_audioPlaybackConnections.emplace(device.Id(), std::pair(device, connection));

			connection.StateChanged([](const auto& sender, const auto&) {
				auto state = sender.State();
				if (state == AudioPlaybackConnectionState::Closed)
				{
					auto deviceId = std::make_unique<std::wstring>(sender.DeviceId().c_str());
					if (!PostMessageW(g_hWnd, WM_CONNECTION_CLOSED, 0, reinterpret_cast<LPARAM>(deviceId.get())))
					{
						LOG_LAST_ERROR();
					}
					else
					{
						deviceId.release();
					}
					sender.Close();
				}
				else if (state != AudioPlaybackConnectionState::Opened)
				{
					auto deviceId = std::make_unique<std::wstring>(sender.DeviceId().c_str());
					if (!PostMessageW(g_hWnd, WM_RECOVERDEVICE, 0, reinterpret_cast<LPARAM>(deviceId.get())))
					{
						LOG_LAST_ERROR();
					}
					else
					{
						deviceId.release();
					}
				}
			});

			co_await connection.StartAsync();
			auto result = co_await connection.OpenAsync();

			switch (result.Status())
			{
			case AudioPlaybackConnectionOpenResultStatus::Success:
				success = true;
				break;
			case AudioPlaybackConnectionOpenResultStatus::RequestTimedOut:
				success = false;
				errorMessage = _(L"The request timed out");
				break;
			case AudioPlaybackConnectionOpenResultStatus::DeniedBySystem:
				success = false;
				errorMessage = _(L"The operation was denied by the system");
				break;
			case AudioPlaybackConnectionOpenResultStatus::UnknownFailure:
				success = false;
				winrt::throw_hresult(result.ExtendedError());
				break;
			default:
				success = false;
				errorMessage = _(L"Unknown error");
				break;
			}
		}
		else
		{
			success = false;
			errorMessage = _(L"Unknown error");
		}
	}
	catch (winrt::hresult_error const& ex)
	{
		success = false;
		errorMessage.resize(64);
		while (1)
		{
			auto result = swprintf(errorMessage.data(), errorMessage.size(), L"%s (0x%08X)", ex.message().c_str(), static_cast<uint32_t>(ex.code()));
			if (result < 0)
			{
				errorMessage.resize(errorMessage.size() * 2);
			}
			else
			{
				errorMessage.resize(result);
				break;
			}
		}
		LOG_CAUGHT_EXCEPTION();
	}

	if (success)
	{
		g_retryCounts.erase(deviceId);
		picker.SetDisplayStatus(device, LinkConnectedStatusText(), DevicePickerDisplayStatusOptions::ShowDisconnectButton);
	}
	else
	{
		auto it = g_audioPlaybackConnections.find(deviceId);
		if (it != g_audioPlaybackConnections.end())
		{
			it->second.second.Close();
			g_audioPlaybackConnections.erase(it);
		}
		if (!QueueReconnect(device, errorMessage))
		{
			picker.SetDisplayStatus(device, errorMessage, DevicePickerDisplayStatusOptions::ShowRetryButton);
		}
	}
}

winrt::fire_and_forget RecoverDevice(std::wstring deviceId)
{
	if (g_isShuttingDown)
	{
		co_return;
	}

	auto [_, inserted] = g_recoveringDevices.emplace(deviceId);
	if (!inserted)
	{
		co_return;
	}

	auto cleanup = wil::scope_exit([&]() {
		g_recoveringDevices.erase(deviceId);
	});

	auto it = g_audioPlaybackConnections.find(deviceId);
	if (it == g_audioPlaybackConnections.end())
	{
		co_return;
	}

	auto device = it->second.first;
	it->second.second.Close();
	g_audioPlaybackConnections.erase(it);
	g_devicePicker.SetDisplayStatus(device, LinkRecoveringStatusText(), DevicePickerDisplayStatusOptions::ShowProgress | DevicePickerDisplayStatusOptions::ShowDisconnectButton);

	co_await winrt::resume_after(std::chrono::milliseconds(600));
	if (g_isShuttingDown)
	{
		co_return;
	}

	QueueReconnect(device);
}

winrt::fire_and_forget ConnectDevice(DevicePicker picker, std::wstring_view deviceId)
{
	try
	{
		auto device = co_await DeviceInformation::CreateFromIdAsync(deviceId);
		ConnectDevice(picker, device);
	}
	catch (winrt::hresult_error const&)
	{
		LOG_CAUGHT_EXCEPTION();
	}
}

void SetupDevicePicker()
{
	g_devicePicker = DevicePicker();
	winrt::check_hresult(g_devicePicker.as<IInitializeWithWindow>()->Initialize(g_hWnd));

	g_devicePicker.Filter().SupportedDeviceSelectors().Append(AudioPlaybackConnection::GetDeviceSelector());
	g_devicePicker.DevicePickerDismissed([](const auto&, const auto&) {
		SetWindowPos(g_hWnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_HIDEWINDOW);
	});
	g_devicePicker.DeviceSelected([](const auto& sender, const auto& args) {
		ConnectDevice(sender, args.SelectedDevice());
	});
	g_devicePicker.DisconnectButtonClicked([](const auto& sender, const auto& args) {
		auto device = args.Device();
		MarkManualDisconnect(device.Id().c_str());
		auto it = g_audioPlaybackConnections.find(std::wstring(device.Id()));
		if (it != g_audioPlaybackConnections.end())
		{
			it->second.second.Close();
			g_audioPlaybackConnections.erase(it);
		}
		sender.SetDisplayStatus(device, {}, DevicePickerDisplayStatusOptions::None);
	});
}

void SetupSvgIcon()
{
	auto hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(1), L"SVG");
	FAIL_FAST_LAST_ERROR_IF_NULL(hRes);

	auto size = SizeofResource(g_hInst, hRes);
	FAIL_FAST_LAST_ERROR_IF(size == 0);

	auto hResData = LoadResource(g_hInst, hRes);
	FAIL_FAST_LAST_ERROR_IF_NULL(hResData);

	auto svgData = reinterpret_cast<const char*>(LockResource(hResData));
	FAIL_FAST_IF_NULL_ALLOC(svgData);

	const std::string_view svg(svgData, size);
	const int width = GetSystemMetrics(SM_CXSMICON), height = GetSystemMetrics(SM_CYSMICON);

	g_hIconLight = SvgTohIcon(svg, width, height, { 0, 0, 0, 1 });
	g_hIconDark = SvgTohIcon(svg, width, height, { 1, 1, 1, 1 });
}

void UpdateNotifyIcon()
{
	DWORD value = 0, cbValue = sizeof(value);
	LOG_IF_WIN32_ERROR(RegGetValueW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &cbValue));
	g_nid.hIcon = value != 0 ? g_hIconLight : g_hIconDark;

	if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid))
	{
		if (Shell_NotifyIconW(NIM_ADD, &g_nid))
		{
			FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &g_nid));
		}
		else
		{
			LOG_LAST_ERROR();
		}
	}
}
