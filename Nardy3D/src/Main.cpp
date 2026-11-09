#include "App.h"

#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try {
        App app(instance);
        return app.Run();
    }
    catch (const std::exception& exception) {
        MessageBoxW(nullptr, win32::Widen(exception.what()).c_str(), L"3D Nardy", MB_ICONERROR | MB_OK);
        return 1;
    }
}
