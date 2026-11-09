#pragma once

#include "Strings.h"
#include "Win32.h"

#include <array>
#include <string>

// The choices and the game in progress, kept between sessions in %APPDATA%\Nardy3D\Nardy3D.ini.
struct Settings {
    int theme = 0;
    int level = 3;
    int mode = 0;
    int game = 0;   // long or short nardy
    int camera = 0; // overview or from above
    bool autoRoll = true;
    bool sound = true;
    bool panel = true;
    Language language = Language::English;
    std::array<int, 2> score{};
    bool hasWindow = false;
    RECT window{};
    bool maximized = false;
    std::string match;   // the position and the turn, as text
    std::string history; // the lines of the game's history, each after its kind, separated by '|'

    static Settings Load();
    void Save() const;
};
