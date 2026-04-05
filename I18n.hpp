#pragma once
#include "FnvHash.hpp"

std::unordered_map<uint32_t, const wchar_t*> hashToStrMap;

#pragma pack(push, 1)
struct YMOData
{
	uint16_t len;
	struct
	{
		uint32_t hash;
		uint16_t offset;
	} table[1];
};
#pragma pack(pop)

inline bool ShouldUseSimplifiedChinese()
{
	switch (g_languageMode)
	{
	case LanguageMode::English:
		return false;
	case LanguageMode::ChineseSimplified:
		return true;
	case LanguageMode::System:
	default:
		return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
	}
}

inline const wchar_t* TranslateManual(const wchar_t* str)
{
	static const std::unordered_map<std::wstring_view, const wchar_t*> zhHansMap = {
		{ L"Unsupported Operating System", L"\u4E0D\u652F\u6301\u7684\u64CD\u4F5C\u7CFB\u7EDF" },
		{ L"AudioPlaybackConnector is not supported on this operating system version.", L"\u5F53\u524D\u64CD\u4F5C\u7CFB\u7EDF\u7248\u672C\u4E0D\u652F\u6301 AudioPlaybackConnector\u3002" },
		{ L"Audio_phone_to_pc is not supported on this operating system version.", L"\u5F53\u524D\u64CD\u4F5C\u7CFB\u7EDF\u7248\u672C\u4E0D\u652F\u6301 Audio_phone_to_pc\u3002" },
		{ L"AudioPlaybackConnector", L"AudioPlaybackConnector" },
		{ L"Audio_phone_to_pc", L"Audio_phone_to_pc" },
		{ L"All connections will be closed.\nExit anyway?", L"\u6240\u6709\u8FDE\u63A5\u90FD\u4F1A\u5173\u95ED\u3002\n\u4ECD\u7136\u9000\u51FA\u5417\uFF1F" },
		{ L"Reconnect on next start", L"\u4E0B\u6B21\u542F\u52A8\u65F6\u81EA\u52A8\u6062\u590D\u8FDE\u63A5" },
		{ L"Exit", L"\u9000\u51FA" },
		{ L"Run at startup", L"\u5F00\u673A\u542F\u52A8" },
		{ L"Allow multiple connected devices", L"\u5141\u8BB8\u540C\u65F6\u8FDE\u63A5\u591A\u4E2A\u8BBE\u5907" },
		{ L"Bluetooth Settings", L"\u84DD\u7259\u8BBE\u7F6E" },
		{ L"Device Settings", L"\u8BBE\u5907\u8BBE\u7F6E" },
		{ L"Sound Settings", L"\u58F0\u97F3\u8BBE\u7F6E" },
		{ L"App Volume Settings", L"\u5E94\u7528\u97F3\u91CF\u8BBE\u7F6E" },
		{ L"Connection Tips", L"\u8FDE\u63A5\u63D0\u793A" },
		{ L"Language", L"\u8BED\u8A00" },
		{ L"Follow system language", L"\u8DDF\u968F\u7CFB\u7EDF\u8BED\u8A00" },
		{ L"English", L"English" },
		{ L"Simplified Chinese", L"\u7B80\u4F53\u4E2D\u6587" },
		{ L"Switching devices will disconnect the previous audio device to keep playback stable.\n\nIf a phone or tablet wakes up slowly, the app will retry automatically a few times for you.", L"\u5207\u6362\u8BBE\u5907\u65F6\uFF0C\u7A0B\u5E8F\u4F1A\u65AD\u5F00\u4E4B\u524D\u7684\u97F3\u9891\u8BBE\u5907\uFF0C\u4EE5\u4FDD\u8BC1\u64AD\u653E\u66F4\u7A33\u5B9A\u3002\n\n\u5982\u679C\u624B\u673A\u6216\u5E73\u677F\u521A\u5524\u9192\u3001\u54CD\u5E94\u8F83\u6162\uFF0C\u7A0B\u5E8F\u4F1A\u81EA\u52A8\u518D\u91CD\u8BD5\u51E0\u6B21\uFF0C\u51CF\u5C11\u9700\u8981\u624B\u52A8\u91CD\u8FDE\u7684\u60C5\u51B5\u3002" },
		{ L"Connecting audio...", L"\u6B63\u5728\u8FDE\u63A5\u97F3\u9891..." },
		{ L"Audio ready", L"\u97F3\u9891\u5DF2\u8FDE\u63A5" },
		{ L"Reconnecting audio...", L"\u6B63\u5728\u91CD\u65B0\u8FDE\u63A5\u97F3\u9891..." },
		{ L"Switched to another device", L"\u5DF2\u5207\u6362\u5230\u5176\u4ED6\u8BBE\u5907" },
		{ L"Connection lost. Retrying automatically...", L"\u8FDE\u63A5\u5DF2\u65AD\u5F00\uFF0C\u6B63\u5728\u81EA\u52A8\u91CD\u8BD5..." },
		{ L"Still trying to reconnect...", L"\u4ECD\u5728\u5C1D\u8BD5\u91CD\u65B0\u8FDE\u63A5..." },
		{ L"Automatic retry stopped. Click to try again.", L"\u81EA\u52A8\u91CD\u8BD5\u5DF2\u505C\u6B62\uFF0C\u70B9\u51FB\u53EF\u518D\u6B21\u5C1D\u8BD5\u3002" },
		{ L"The request timed out", L"\u8BF7\u6C42\u8D85\u65F6" },
		{ L"The operation was denied by the system", L"\u7CFB\u7EDF\u62D2\u7EDD\u4E86\u6B64\u6B21\u64CD\u4F5C" },
		{ L"Unknown error", L"\u672A\u77E5\u9519\u8BEF" }
	};

	auto it = zhHansMap.find(str);
	if (it != zhHansMap.end())
	{
		return it->second;
	}
	return str;
}

void LoadTranslateData()
{
	hashToStrMap.clear();

	auto hRes = FindResourceExW(g_hInst, L"YMO", MAKEINTRESOURCEW(1), MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
	if (!hRes)
	{
		return;
	}

	auto resSize = SizeofResource(g_hInst, hRes);
	if (resSize < sizeof(uint16_t))
	{
		return;
	}

	auto hResData = LoadResource(g_hInst, hRes);
	if (!hResData)
	{
		return;
	}

	auto resourceData = reinterpret_cast<const uint8_t*>(LockResource(hResData));
	auto ymo = reinterpret_cast<const YMOData*>(resourceData);
	if (!ymo)
	{
		return;
	}

	hashToStrMap.reserve(ymo->len);
	for (int i = 0; i < ymo->len; ++i)
	{
		auto entryEnd = sizeof(uint16_t) + (i + 1) * (sizeof(uint32_t) + sizeof(uint16_t));
		if (entryEnd > resSize)
		{
			break;
		}

		auto hash = ymo->table[i].hash;
		auto offset = ymo->table[i].offset;
		if (offset >= resSize)
		{
			continue;
		}

		auto translated = reinterpret_cast<const wchar_t*>(resourceData + offset);
		hashToStrMap.emplace(hash, translated);
	}
}

const wchar_t* Translate(const wchar_t* str)
{
	if (!ShouldUseSimplifiedChinese())
	{
		return str;
	}

	auto translated = TranslateManual(str);
	if (translated != str)
	{
		return translated;
	}

	auto hash = fnv1a_32(str, wcslen(str) * sizeof(wchar_t));
	auto it = hashToStrMap.find(hash);
	if (it != hashToStrMap.end())
	{
		return it->second;
	}

	return str;
}

const wchar_t* TranslateContext(const wchar_t* str, const wchar_t* ctxtStr)
{
	auto translation = Translate(ctxtStr);
	if (translation == ctxtStr)
		return Translate(str);
	return translation;
}

#define _(str) Translate(str)
#define C_(ctxt, str) TranslateContext(str, ctxt L"\004" str)
