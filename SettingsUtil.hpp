#pragma once

constexpr auto CONFIG_NAME = L"AudioPlaybackConnector.json";
constexpr auto BUFFER_SIZE = 4096;

void DefaultSettings()
{
	g_reconnect = false;
	g_preferBestLink = false;
	g_allowMultiDevice = true;
	g_languageMode = LanguageMode::System;
	g_lastDevices.clear();
}

void LoadSettings()
{
	try
	{
		DefaultSettings();

		wil::unique_hfile hFile(CreateFileW((GetModuleFsPath(g_hInst).remove_filename() / CONFIG_NAME).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
		THROW_LAST_ERROR_IF(!hFile);

		std::string string;
		while (1)
		{
			size_t size = string.size();
			string.resize(size + BUFFER_SIZE);
			DWORD read = 0;
			THROW_IF_WIN32_BOOL_FALSE(ReadFile(hFile.get(), string.data() + size, BUFFER_SIZE, &read, nullptr));
			string.resize(size + read);
			if (read == 0)
				break;
		}

		std::wstring utf16 = Utf8ToUtf16(string);
		auto jsonObj = JsonObject::Parse(utf16);
		auto reconnectValue = jsonObj.TryLookup(L"reconnect");
		if (reconnectValue && reconnectValue.ValueType() == JsonValueType::Boolean)
			g_reconnect = reconnectValue.GetBoolean();

		auto languageValue = jsonObj.TryLookup(L"language");
		if (languageValue && languageValue.ValueType() == JsonValueType::String)
		{
			auto language = languageValue.GetString();
			if (language == L"en")
				g_languageMode = LanguageMode::English;
			else if (language == L"zh-CN")
				g_languageMode = LanguageMode::ChineseSimplified;
			else
				g_languageMode = LanguageMode::System;
		}

		auto preferBestLinkValue = jsonObj.TryLookup(L"preferBestLink");
		if (preferBestLinkValue && preferBestLinkValue.ValueType() == JsonValueType::Boolean)
			g_preferBestLink = preferBestLinkValue.GetBoolean();

		auto allowMultiDeviceValue = jsonObj.TryLookup(L"allowMultiDevice");
		if (allowMultiDeviceValue && allowMultiDeviceValue.ValueType() == JsonValueType::Boolean)
		{
			g_allowMultiDevice = allowMultiDeviceValue.GetBoolean();
			g_preferBestLink = !g_allowMultiDevice;
		}
		else
		{
			// Backward compatibility with old preferBestLink setting.
			g_allowMultiDevice = !g_preferBestLink;
		}

		auto lastDevicesValue = jsonObj.TryLookup(L"lastDevices");
		if (lastDevicesValue && lastDevicesValue.ValueType() == JsonValueType::Array)
		{
			auto lastDevices = lastDevicesValue.GetArray();
			g_lastDevices.reserve(lastDevices.Size());
			for (const auto& i : lastDevices)
			{
				if (i.ValueType() == JsonValueType::String)
					g_lastDevices.push_back(std::wstring(i.GetString()));
			}
		}
	}
	CATCH_LOG();
}

void SaveSettings()
{
	try
	{
		JsonObject jsonObj;
		jsonObj.Insert(L"reconnect", JsonValue::CreateBooleanValue(g_reconnect));
		jsonObj.Insert(L"preferBestLink", JsonValue::CreateBooleanValue(g_preferBestLink));
		jsonObj.Insert(L"allowMultiDevice", JsonValue::CreateBooleanValue(g_allowMultiDevice));
		switch (g_languageMode)
		{
		case LanguageMode::English:
			jsonObj.Insert(L"language", JsonValue::CreateStringValue(L"en"));
			break;
		case LanguageMode::ChineseSimplified:
			jsonObj.Insert(L"language", JsonValue::CreateStringValue(L"zh-CN"));
			break;
		case LanguageMode::System:
		default:
			jsonObj.Insert(L"language", JsonValue::CreateStringValue(L"system"));
			break;
		}

		JsonArray lastDevices;
		for (const auto& i : g_audioPlaybackConnections)
		{
			lastDevices.Append(JsonValue::CreateStringValue(i.first));
		}
		jsonObj.Insert(L"lastDevices", lastDevices);

		wil::unique_hfile hFile(CreateFileW((GetModuleFsPath(g_hInst).remove_filename() / CONFIG_NAME).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
		THROW_LAST_ERROR_IF(!hFile);

		std::string utf8 = Utf16ToUtf8(jsonObj.Stringify());
		DWORD written = 0;
		THROW_IF_WIN32_BOOL_FALSE(WriteFile(hFile.get(), utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr));
		THROW_HR_IF(E_FAIL, written != utf8.size());
	}
	CATCH_LOG();
}
