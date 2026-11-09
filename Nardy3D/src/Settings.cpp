#include "Settings.h"

#include "Themes.h"

#include <algorithm>
#include <vector>

namespace {

constexpr const wchar_t* Section = L"Nardy3D";

std::wstring FilePath()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
    std::wstring folder = length > 0 && length < MAX_PATH ? std::wstring(buffer, length) : std::wstring(L".");
    folder += L"\\Nardy3D";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"\\Nardy3D.ini";
}

int ReadInt(const std::wstring& file, const wchar_t* key, int fallback)
{
    return static_cast<int>(GetPrivateProfileIntW(Section, key, fallback, file.c_str()));
}

void WriteInt(const std::wstring& file, const wchar_t* key, int value)
{
    WritePrivateProfileStringW(Section, key, std::to_wstring(value).c_str(), file.c_str());
}

Language SystemLanguage()
{
    const LANGID language = PRIMARYLANGID(GetUserDefaultUILanguage());
    return language == LANG_RUSSIAN || language == LANG_UKRAINIAN || language == LANG_BELARUSIAN ? Language::Russian : Language::English;
}

} // namespace

Settings Settings::Load()
{
    const std::wstring file = FilePath();
    Settings settings;
    settings.theme = std::clamp(ReadInt(file, L"Theme", 0), 0, ThemeCount - 1);
    settings.level = std::clamp(ReadInt(file, L"Level", 3), 1, 5);
    settings.mode = std::clamp(ReadInt(file, L"Mode", 0), 0, 2);
    settings.game = std::clamp(ReadInt(file, L"Game", 0), 0, 1);
    settings.camera = std::clamp(ReadInt(file, L"Camera", 0), 0, 1);
    settings.autoRoll = ReadInt(file, L"AutoRoll", 1) != 0;
    settings.score = {std::max(0, ReadInt(file, L"ScoreWhite", 0)), std::max(0, ReadInt(file, L"ScoreBlack", 0))};
    settings.sound = ReadInt(file, L"Sound", 1) != 0;
    settings.panel = ReadInt(file, L"Panel", 1) != 0;
    const int language = ReadInt(file, L"Language", -1);
    settings.language = language < 0 ? SystemLanguage() : language == 1 ? Language::Russian : Language::English;

    const int width = ReadInt(file, L"WindowWidth", 0), height = ReadInt(file, L"WindowHeight", 0);
    if (width > 0 && height > 0) {
        settings.hasWindow = true;
        settings.window.left = ReadInt(file, L"WindowLeft", 0);
        settings.window.top = ReadInt(file, L"WindowTop", 0);
        settings.window.right = settings.window.left + width;
        settings.window.bottom = settings.window.top + height;
        settings.maximized = ReadInt(file, L"Maximized", 0) != 0;
    }

    std::vector<wchar_t> match(4096);
    const DWORD length = GetPrivateProfileStringW(Section, L"Match", L"", match.data(), static_cast<DWORD>(match.size()), file.c_str());
    settings.match = win32::Narrow(std::wstring_view(match.data(), length));
    std::vector<wchar_t> history(32768);
    const DWORD historyLength = GetPrivateProfileStringW(Section, L"History", L"", history.data(), static_cast<DWORD>(history.size()), file.c_str());
    settings.history = win32::Narrow(std::wstring_view(history.data(), historyLength));
    return settings;
}

void Settings::Save() const
{
    const std::wstring file = FilePath();
    WriteInt(file, L"Theme", theme);
    WriteInt(file, L"Level", level);
    WriteInt(file, L"Mode", mode);
    WriteInt(file, L"Game", game);
    WriteInt(file, L"Camera", camera);
    WriteInt(file, L"AutoRoll", autoRoll ? 1 : 0);
    WriteInt(file, L"ScoreWhite", score[0]);
    WriteInt(file, L"ScoreBlack", score[1]);
    WriteInt(file, L"Sound", sound ? 1 : 0);
    WriteInt(file, L"Panel", panel ? 1 : 0);
    WriteInt(file, L"Language", language == Language::Russian ? 1 : 0);
    if (hasWindow) {
        WriteInt(file, L"WindowLeft", window.left);
        WriteInt(file, L"WindowTop", window.top);
        WriteInt(file, L"WindowWidth", window.right - window.left);
        WriteInt(file, L"WindowHeight", window.bottom - window.top);
        WriteInt(file, L"Maximized", maximized ? 1 : 0);
    }
    WritePrivateProfileStringW(Section, L"Match", win32::Widen(match).c_str(), file.c_str());
    WritePrivateProfileStringW(Section, L"History", win32::Widen(history).c_str(), file.c_str());
}
