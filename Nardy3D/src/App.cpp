#include "App.h"

#include "BoardModel.h"
#include "Resources.h"
#include "Themes.h"

#include <dwmapi.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

using namespace math;
using nardy::Step;
using ui::FontId;
using ui::Rect;
using ui::Rgba;

namespace {

constexpr wchar_t ClassName[] = L"Nardy3D.Window";
constexpr float PanelWidth = 352, PanelMargin = 16;
constexpr float MinThink = 0.5f;   // seconds the computer takes over a move, at least
constexpr float StepPause = 0.45f; // between the computer's steps
constexpr float TurnPause = 0.55f; // after the last step, before the turn passes
constexpr float PassPause = 1.6f;  // when nothing can be played
constexpr float DragStart = 6;     // pixels the pointer travels before a press becomes a drag

enum Widget {
    HelpButton = 1,
    RollButton,
    HintButton,
    UndoButton,
    NewGameButton,
    AutoRollButton,
    CameraButton,
    SoundButton,
    LanguageButton,
    GameSelector,
    ModeSelector = GameSelector + 2,
    LevelSelector = ModeSelector + 2,
    ThemeSelector = LevelSelector + 2,
    ModalWidgets = 100, // the widgets of the help
    HelpClose,
};

constexpr ui::Color TextColor = Rgba(236, 239, 245);
constexpr ui::Color MutedColor = Rgba(150, 158, 176);
constexpr ui::Color NoteColor = Rgba(255, 200, 130);

// The dice of a roll in the order they are played: two, or four of a double.
std::vector<int> DiceOf(nardy::Roll roll)
{
    if (roll.Double())
        return {roll.first, roll.first, roll.first, roll.first};
    return {roll.first, roll.second};
}

// Whether the move's steps begin with `prefix`.
bool StartsWith(const nardy::Move& move, const std::vector<Step>& prefix)
{
    if (move.count < static_cast<int>(prefix.size()))
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (!(move.steps[i] == prefix[i]))
            return false;
    return true;
}

} // namespace

// Setup --------------------------------------------------------------------------------------------

App::App(HINSTANCE instance) : m_instance(instance)
{
    m_settings = Settings::Load();
    m_language = m_settings.language;
    m_theme = m_settings.theme;
    m_level = m_settings.level;
    m_mode = m_settings.mode;
    m_game = m_settings.game;
    m_autoRoll = m_settings.autoRoll;
    m_score = m_settings.score;
    m_sound.enabled = m_settings.sound;
    m_panelVisible = m_settings.panel;
    m_panelSlide = m_panelVisible ? 1.0f : 0.0f;
    m_random.seed((static_cast<std::uint64_t>(std::random_device{}()) << 32) ^
                  static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));

    CreateMainWindow();
    m_context = std::make_unique<gl::Context>(m_window);
    render::Program::SetVersion(m_context->Version());
    m_painter = std::make_unique<ui::Painter>();
    BuildFonts();
    ShowWindow(m_window, m_settings.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(m_window);
    ShowLoading();

    m_renderer = std::make_unique<render::Renderer>(m_context->Version());
    m_renderer->SetTheme(Themes()[static_cast<std::size_t>(m_theme)]);
    m_camera.SetMode(static_cast<CameraMode>(m_settings.camera));
    LoadNetworks();
    if (LoadGame(m_settings.match)) {
        // The history of the game, as far as it was kept.
        std::string_view history = m_settings.history;
        while (!history.empty()) {
            const std::size_t end = std::min(history.find('|'), history.size());
            const std::string_view line = history.substr(0, end);
            if (line.size() > 1 && line[0] >= '0' && line[0] <= '3')
                m_history.push_back({std::string(line.substr(1)), line[0] - '0'});
            history.remove_prefix(std::min(end + 1, history.size()));
        }
    }
    else {
        NewGame();
    }
    m_lastFrame = std::chrono::steady_clock::now();
}

App::~App()
{
    StopComputer();
}

void App::CreateMainWindow()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = m_instance;
    windowClass.hIcon = LoadIconW(m_instance, MAKEINTRESOURCEW(1));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = ClassName;
    RegisterClassExW(&windowClass);

    RECT rect{};
    if (m_settings.hasWindow && MonitorFromRect(&m_settings.window, MONITOR_DEFAULTTONULL)) {
        rect = m_settings.window;
    }
    else {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int dpi = static_cast<int>(GetDpiForSystem());
        const int workWidth = work.right - work.left, workHeight = work.bottom - work.top;
        const int width = std::min(MulDiv(1500, dpi, 96), workWidth * 92 / 100);
        const int height = std::min(MulDiv(900, dpi, 96), workHeight * 92 / 100);
        rect = {work.left + (workWidth - width) / 2, work.top + (workHeight - height) / 2, 0, 0};
        rect.right = rect.left + width;
        rect.bottom = rect.top + height;
    }

    m_window = CreateWindowExW(0, ClassName, win32::Widen(T(Str::Title)).c_str(), WS_OVERLAPPEDWINDOW, rect.left, rect.top, rect.right - rect.left,
                               rect.bottom - rect.top, nullptr, nullptr, m_instance, this);
    if (!m_window)
        throw std::runtime_error("The window could not be created.");
    m_dpi = GetDpiForWindow(m_window);
    m_placement.length = sizeof(m_placement);
    // A dark title bar, to go with the game, where Windows offers one.
    constexpr DWORD UseImmersiveDarkMode = 20; // DWMWA_USE_IMMERSIVE_DARK_MODE, Windows 10 20H1+
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(m_window, UseImmersiveDarkMode, &dark, sizeof(dark));
}

void App::BuildFonts()
{
    const float scale = Scale();
    m_fonts[0] = std::make_unique<text::Font>(L"Segoe UI", 15 * scale, FW_NORMAL);
    m_fonts[1] = std::make_unique<text::Font>(L"Segoe UI", 19 * scale, FW_SEMIBOLD);
    m_fonts[2] = std::make_unique<text::Font>(L"Segoe UI", 31 * scale, FW_SEMIBOLD);
    m_painter->SetFonts(m_fonts[0].get(), m_fonts[1].get(), m_fonts[2].get());
    m_fontDpi = m_dpi;
}

void App::ShowLoading()
{
    RECT client{};
    GetClientRect(m_window, &client);
    m_width = client.right;
    m_height = client.bottom;
    gl::glBindFramebuffer(gl::GL_FRAMEBUFFER, 0);
    gl::glViewport(0, 0, m_width, m_height);
    gl::glClearColor(0.014f, 0.012f, 0.01f, 1);
    gl::glClear(gl::GL_COLOR_BUFFER_BIT);
    m_painter->Begin(m_width, m_height);
    const Rect screen{0, 0, static_cast<float>(m_width), static_cast<float>(m_height)};
    m_painter->TextCentered(FontId::Display, screen, T(Str::Title), TextColor);
    m_painter->Flush();
    m_context->SwapBuffers();
}

// The computer's judgement of positions, one network for each game, built into the program.
void App::LoadNetworks()
{
    const std::array<const wchar_t*, 2> names{L"LONG_NET", L"SHORT_NET"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::string_view data = resources::Text(names[i]);
        if (!m_nets[i].Load(std::as_bytes(std::span(data.data(), data.size()))))
            throw std::runtime_error("The computer player's network in the program's resources is damaged.");
    }
}

int App::Run()
{
    MSG message{};
    while (m_running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!m_running)
            break;
        if (IsIconic(m_window)) {
            WaitMessage();
            m_lastFrame = std::chrono::steady_clock::now();
            continue;
        }

        // Nothing moving and nothing new: wait for input instead of drawing the same picture again.
        if (!m_dirty && !NeedsAnimation()) {
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            m_lastFrame = std::chrono::steady_clock::now(); // the idle time is not animation time
            continue;
        }
        m_dirty = false;
        const auto start = std::chrono::steady_clock::now();
        Frame();
        // Swapping the buffers waits for the screen; where it does not, the loop sleeps a little.
        if (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2))
            Sleep(1);
    }
    return 0;
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        static_cast<App*>(create->lpCreateParams)->m_window = window;
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return app ? app->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    m_dirty = true;
    auto pointerAt = [&] {
        m_pointer.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        m_pointer.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
    };

    switch (message) {
    case WM_CLOSE:
        Save();
        DestroyWindow(m_window);
        return 0;
    case WM_DESTROY:
        m_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(m_window, &paint);
        EndPaint(m_window, &paint);
        // While the window is being resized the loop does not run; the frames come from here.
        if (m_renderer && m_width > 0 && m_height > 0)
            Frame();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED: {
        m_dpi = HIWORD(wParam);
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(m_window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {MulDiv(960, static_cast<int>(m_dpi), 96), MulDiv(680, static_cast<int>(m_dpi), 96)};
        return 0;
    }
    case WM_MOUSEMOVE:
        pointerAt();
        if (!m_trackingLeave) {
            // To hear when the pointer leaves the window, so that nothing stays highlighted under it.
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, m_window, 0};
            m_trackingLeave = TrackMouseEvent(&track) != FALSE;
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        pointerAt();
        SetCapture(m_window);
        m_pointer.leftDown = m_pointer.leftPressed = true;
        m_pointer.doubleClick = message == WM_LBUTTONDBLCLK;
        return 0;
    case WM_LBUTTONUP:
        pointerAt();
        m_pointer.leftDown = false;
        m_pointer.leftReleased = true;
        if (!m_pointer.rightDown)
            ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        pointerAt();
        SetCapture(m_window);
        m_pointer.rightDown = m_pointer.rightPressed = true;
        return 0;
    case WM_RBUTTONUP:
        pointerAt();
        m_pointer.rightDown = false;
        m_pointer.rightReleased = true;
        if (!m_pointer.leftDown)
            ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        if (m_pointer.leftDown)
            m_pointer.leftReleased = true;
        if (m_pointer.rightDown)
            m_pointer.rightReleased = true;
        m_pointer.leftDown = m_pointer.rightDown = false;
        return 0;
    case WM_MOUSELEAVE:
        m_trackingLeave = false;
        if (!m_pointer.leftDown && !m_pointer.rightDown)
            m_pointer.x = m_pointer.y = -1;
        return 0;
    case WM_MOUSEWHEEL:
        m_pointer.wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        return 0;
    case WM_KEYDOWN:
        HandleKey(wParam);
        return 0;
    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (HIWORD(lParam) & KF_ALTDOWN)) {
            ToggleFullScreen();
            return 0;
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, m_cursorHand ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(m_window, message, wParam, lParam);
}

// The frame ----------------------------------------------------------------------------------------

void App::Frame()
{
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::min(std::chrono::duration<float>(now - m_lastFrame).count(), 0.1f);
    m_lastFrame = now;
    m_time += elapsed;
    if (m_width <= 0 || m_height <= 0)
        return;
    if (m_dpi != m_fontDpi)
        BuildFonts();
    m_renderer->Resize(m_width, m_height);

    Update(elapsed);
    m_renderer->Render(BuildScene());

    m_cursorHand = false;
    m_painter->Begin(m_width, m_height);
    DrawInterface();
    m_painter->Flush();
    HandleBoardPointer();

    m_pointer.leftPressed = m_pointer.leftReleased = m_pointer.rightPressed = m_pointer.rightReleased = false;
    m_pointer.doubleClick = false;
    m_pointer.wheel = 0;
    m_pointer.consumed = false;
    m_context->SwapBuffers();
}

void App::Update(float seconds)
{
    const float slideTarget = m_panelVisible ? 1.0f : 0.0f;
    m_panelSlide += (slideTarget - m_panelSlide) * (1 - std::exp(-seconds * 12));
    if (std::abs(slideTarget - m_panelSlide) < 0.001f)
        m_panelSlide = slideTarget;
    m_camera.SetSpace(FreeArea().width / std::max(1.0f, static_cast<float>(m_width)));

    m_stageClock += seconds;
    switch (m_stage) {
    case Stage::Opening:
        if (!m_view.DiceRolling() && m_stageClock > 1.6f)
            FinishOpening();
        break;
    case Stage::ToRoll:
        if ((!IsHuman(m_toPlay) && m_stageClock > 0.45f) || (IsHuman(m_toPlay) && m_autoRoll && m_stageClock > 0.3f))
            RollDice();
        break;
    case Stage::Rolling:
        if (!m_view.DiceRolling())
            BeginMoves();
        break;
    case Stage::Moving:
        if (m_doneClock > 0) {
            m_doneClock -= seconds;
            if (m_doneClock <= 0)
                EndTurn();
        }
        break;
    case Stage::Thinking:
    case Stage::Playing:
        UpdateComputer(seconds);
        break;
    case Stage::Over:
        if (m_mode == Demo && m_stageClock > 6)
            NewGame();
        break;
    }
    UpdateHint();
    m_view.Update(seconds);
    PlaySounds();
    m_camera.Update(seconds);

    for (Toast& toast : m_toasts)
        toast.age += seconds;
    std::erase_if(m_toasts, [](const Toast& toast) { return toast.age > 2.8f; });
    float bannerTarget = 0;
    if (m_bannerTime < 0) {
        bannerTarget = 1;
    }
    else if (m_bannerTime > 0) {
        bannerTarget = 1;
        m_bannerTime = std::max(0.0f, m_bannerTime - seconds);
    }
    m_bannerFade += (bannerTarget - m_bannerFade) * (1 - std::exp(-seconds * 7));
    if (std::abs(bannerTarget - m_bannerFade) < 0.002f)
        m_bannerFade = bannerTarget;
}

void App::PlaySounds()
{
    const BoardView::Sounds sounds = m_view.TakeSounds();
    std::uniform_real_distribution<float> vary(0.93f, 1.07f);
    using Effect = SoundBank::Effect;
    for (int i = 0; i < std::min(sounds.landings, 3); ++i)
        m_sound.Play(Effect::Checker, 0.8f, 0, vary(m_random));
    for (int i = 0; i < std::min(sounds.hits, 2); ++i)
        m_sound.Play(Effect::Hit, 0.9f, 0, vary(m_random));
    for (int i = 0; i < std::min(sounds.trays, 3); ++i)
        m_sound.Play(Effect::Tray, 0.8f, 0.4f, vary(m_random));
    for (const float volume : sounds.dice)
        m_sound.Play(Effect::Die, volume, 0, vary(m_random));
}

bool App::NeedsAnimation() const
{
    const bool waiting = m_stage == Stage::Opening || m_stage == Stage::Rolling || m_stage == Stage::Thinking || m_stage == Stage::Playing ||
                         (m_stage == Stage::ToRoll && (!IsHuman(m_toPlay) || m_autoRoll)) || (m_stage == Stage::Moving && m_doneClock > 0) ||
                         (m_stage == Stage::Over && m_mode == Demo);
    // The places a checker may go pulse, and so does the hint.
    const bool pulsing = HumanToMove() && (m_selected >= 0 || m_hint.has_value());
    return waiting || pulsing || m_hintWanted || m_camera.Moving() || m_view.Animating() || !m_toasts.empty() || m_bannerTime > 0 ||
           (m_bannerFade > 0.001f && m_bannerFade < 0.999f) || std::abs(m_panelSlide - (m_panelVisible ? 1.0f : 0.0f)) > 0.001f;
}

render::Scene App::BuildScene() const
{
    render::Scene scene;
    const float aspect = static_cast<float>(m_width) / std::max(1.0f, static_cast<float>(m_height));
    scene.view = m_camera.View();
    scene.projection = m_camera.Projection(aspect);
    scene.eye = m_camera.Eye();
    scene.time = m_time;

    auto mark = [&](int place, int component, float value) {
        if (place >= 0 && place < nardy::Points) {
            Vec4& p = scene.points[static_cast<std::size_t>(place)];
            float* values[4] = {&p.x, &p.y, &p.z, &p.w};
            *values[component] = std::max(*values[component], value);
        }
    };
    // The last turn's steps, until the next player moves.
    if (m_played.empty() || m_stage == Stage::Over)
        for (const int place : m_lastMove)
            mark(place, 2, 1);

    std::array<float, BoardView::Places> glow{};
    int glowSide = -1;
    if (HumanToMove() && m_doneClock <= 0) {
        glowSide = m_toPlay;
        for (const Step& step : NextSteps()) {
            const int place = ToPlace(step.from);
            glow[static_cast<std::size_t>(place)] = std::max(glow[static_cast<std::size_t>(place)], m_selected < 0 ? 0.32f : 0.16f);
            if (m_selected < 0)
                mark(place, 1, 1);
        }
        if (m_hoverPlace >= 0 && glow[static_cast<std::size_t>(m_hoverPlace)] > 0)
            glow[static_cast<std::size_t>(m_hoverPlace)] = 0.7f;
        if (m_selected >= 0) {
            for (const Route& route : RoutesFrom(FromPlace(m_selected))) {
                const int place = ToPlace(route.to);
                const float strength = place == m_hoverPlace ? 1.6f : 1.0f;
                if (place == BoardView::OffPlace)
                    (m_toPlay == 0 ? scene.trays.x : scene.trays.y) = strength;
                else
                    mark(place, 0, strength);
            }
        }
    }
    if (m_hint && HumanToMove()) {
        for (int i = 0; i < m_hint->count; ++i) {
            const Step& step = m_hint->steps[static_cast<std::size_t>(i)];
            mark(ToPlace(step.from), 3, 1);
            if (step.to == nardy::BearOff)
                scene.trays.z = m_toPlay == 0 ? 1.0f : -1.0f;
            else
                mark(ToPlace(step.to), 3, 1);
        }
    }

    std::vector<BoardView::Piece> pieces;
    m_view.AddPieces(pieces, glow, glowSide);
    scene.pieces.reserve(pieces.size());
    for (const BoardView::Piece& piece : pieces)
        scene.pieces.push_back({piece.model, piece.kind, piece.side, piece.value, piece.highlight, piece.dim});
    scene.panels = FrostedPanels();
    return scene;
}

// The game -----------------------------------------------------------------------------------------

bool App::IsHuman(int side) const
{
    switch (m_mode) {
    case VsComputer:
        return side == 0;
    case TwoPlayers:
        return true;
    default:
        return false;
    }
}

nardy::Position App::MoverView() const
{
    return m_toPlay == 0 ? m_board : nardy::Flip(m_board, GameVariant());
}

void App::SetFromMover(const nardy::Position& mover)
{
    m_board = m_toPlay == 0 ? mover : nardy::Flip(mover, GameVariant());
}

int App::ToPlace(int point) const
{
    if (point == nardy::FromBar)
        return BoardView::BarPlace;
    if (point == nardy::BearOff)
        return BoardView::OffPlace;
    if (m_toPlay == 0)
        return point;
    // Black counts the points from the other end: short nardy the opposite way round, long nardy from the far side.
    return GameVariant() == nardy::Variant::Short ? nardy::Points - 1 - point : (point + 12) % nardy::Points;
}

int App::FromPlace(int place) const
{
    if (place == BoardView::BarPlace)
        return nardy::FromBar;
    if (place == BoardView::OffPlace)
        return nardy::BearOff;
    if (place < 0)
        return -100;
    return ToPlace(place); // the same reckoning, both ways
}

// A game in progress as text: the game, who is to play, the first moves still to make, the dice if thrown, and the
// board as White sees it when the turn began.
std::string App::SaveGame() const
{
    if (m_stage == Stage::Opening || m_stage == Stage::Over)
        return {};
    const nardy::Position& board = m_rolled ? m_turnStart : m_board;
    std::string text = std::format("{} {} {} {} {} {} {}", m_game, m_toPlay, m_firstMove[0] ? 1 : 0, m_firstMove[1] ? 1 : 0, m_rolled ? 1 : 0, m_roll.first,
                                   m_roll.second);
    for (const std::int8_t n : board.points)
        text += std::format(" {}", n);
    text += std::format(" {} {} {} {}", board.bar[0], board.bar[1], board.off[0], board.off[1]);
    return text;
}

bool App::LoadGame(const std::string& text)
{
    std::vector<int> values;
    const char* p = text.data();
    const char* end = text.data() + text.size();
    while (p < end) {
        while (p < end && *p == ' ')
            ++p;
        if (p == end)
            break;
        int value = 0;
        const auto [next, error] = std::from_chars(p, end, value);
        if (error != std::errc())
            return false;
        values.push_back(value);
        p = next;
    }
    if (values.size() != 35 || values[0] != m_game || values[1] < 0 || values[1] > 1 || values[5] < 1 || values[5] > 6 || values[6] < 1 || values[6] > 6)
        return false;
    nardy::Position board;
    std::array<int, 2> count{};
    for (std::size_t i = 0; i < nardy::Points; ++i) {
        const int n = values[7 + i];
        if (n < -nardy::Checkers || n > nardy::Checkers)
            return false;
        board.points[i] = static_cast<std::int8_t>(n);
        count[n > 0 ? 0 : 1] += std::abs(n);
    }
    for (std::size_t side = 0; side < 2; ++side) {
        const int bar = values[31 + side], off = values[33 + side];
        if (bar < 0 || off < 0 || off >= nardy::Checkers || (bar > 0 && GameVariant() == nardy::Variant::Long))
            return false;
        board.bar[side] = static_cast<std::int8_t>(bar);
        board.off[side] = static_cast<std::int8_t>(off);
        count[side] += bar + off;
    }
    if (count[0] != nardy::Checkers || count[1] != nardy::Checkers)
        return false;

    m_board = board;
    m_toPlay = values[1];
    m_firstMove = {values[2] != 0, values[3] != 0};
    m_roll = {values[5], values[6]};
    m_winner = -1;
    m_view.Reset(m_board);
    m_view.ClearDice();
    m_turn = 1;
    if (values[4] != 0) {
        m_rolled = true;
        m_turnStart = m_board;
        m_view.ShowDice(m_toPlay, m_roll.first, m_roll.second);
        BeginMoves();
    }
    else {
        StartTurn();
    }
    return true;
}

void App::NewGame()
{
    StopComputer();
    m_board = nardy::Start(GameVariant());
    m_firstMove = {true, true};
    m_rolled = false;
    m_played.clear();
    m_sequences.clear();
    m_lastMove.clear();
    m_winner = -1;
    m_winPoints = 0;
    m_turn = 0;
    m_doneClock = -1;
    m_undo.clear();
    m_history.clear();
    m_historyScroll = 0;
    m_followHistory = true;
    m_bannerTime = 0;
    Select(-1);
    m_view.Reset(m_board);
    m_view.ClearDice();
    m_sound.Play(SoundBank::Effect::Setup);
    ThrowOpening();
    Save();
}

void App::ThrowOpening()
{
    std::uniform_int_distribution<int> die(1, 6);
    m_opening = {die(m_random), die(m_random)};
    m_view.ThrowOpening(m_opening[0], m_opening[1], m_random);
    m_sound.Play(SoundBank::Effect::Rattle);
    m_stage = Stage::Opening;
    m_stageClock = 0;
}

// The dice of the opening have settled: the higher starts, or both throw again.
void App::FinishOpening()
{
    if (m_opening[0] == m_opening[1]) {
        ShowToast(std::vformat(T(Str::OpeningTie), std::make_format_args(m_opening[0], m_opening[1])));
        ThrowOpening();
        return;
    }
    m_toPlay = m_opening[0] > m_opening[1] ? 0 : 1;
    const std::string_view starts = Pick(m_toPlay, Str::StartsYou, Str::StartsComputer, Str::StartsWhite, Str::StartsBlack);
    ShowToast(std::string(starts));
    AddHistory(std::format("{} ({}:{})", starts, m_opening[0], m_opening[1]), 2);
    if (GameVariant() == nardy::Variant::Short) {
        // In short nardy the opening dice are the first roll.
        ++m_turn;
        m_roll = {m_opening[0], m_opening[1]};
        m_rolled = true;
        m_turnStart = m_board;
        BeginMoves();
    }
    else {
        StartTurn();
    }
    Save();
}

void App::StartTurn()
{
    ++m_turn;
    m_stage = Stage::ToRoll;
    m_stageClock = 0;
    m_rolled = false;
    m_played.clear();
    m_sequences.clear();
    m_doneClock = -1;
    m_hint.reset();
    m_hintWanted = false;
    Select(-1);
}

void App::RollDice()
{
    if (m_stage != Stage::ToRoll)
        return;
    std::uniform_int_distribution<int> die(1, 6);
    m_roll = {die(m_random), die(m_random)};
    m_rolled = true;
    m_turnStart = m_board;
    m_view.Throw(m_toPlay, m_roll.first, m_roll.second, m_random);
    m_sound.Play(SoundBank::Effect::Rattle);
    m_stage = Stage::Rolling;
    m_stageClock = 0;
}

// The dice lie still: every way to play them is found, and the player to move takes over.
void App::BeginMoves()
{
    m_turnStart = m_board;
    m_played.clear();
    m_sequences = nardy::LegalSequences(MoverView(), GameVariant(), m_roll, m_firstMove[static_cast<std::size_t>(m_toPlay)]);
    m_doneClock = -1;
    m_hint.reset();
    m_hintWanted = false;
    m_hintWorker.Cancel();
    Select(-1);
    m_view.UseDice(0, m_roll.Double(), 0);
    m_stageClock = 0;
    if (m_sequences.empty()) {
        // Nothing can be played: the turn passes.
        m_stage = Stage::Moving;
        m_doneClock = PassPause;
        ShowToast(std::string(T(Str::NoMovesToast)));
        m_sound.Play(SoundBank::Effect::NoMove);
        return;
    }
    if (IsHuman(m_toPlay)) {
        if (!m_undo.empty() && m_undo.back().turn >= m_turn)
            m_undo.pop_back();
        m_undo.push_back({m_board, m_toPlay, m_firstMove, m_roll, m_history.size(), m_turn});
        if (m_undo.size() > 300)
            m_undo.erase(m_undo.begin());
        m_stage = Stage::Moving;
        return;
    }
    m_stage = Stage::Thinking;
    m_computerMove.reset();
    const nardy::Network* net = &Net();
    const nardy::Position position = MoverView();
    const nardy::Variant variant = GameVariant();
    const nardy::Roll roll = m_roll;
    const bool first = m_firstMove[static_cast<std::size_t>(m_toPlay)], opponentFirst = m_firstMove[static_cast<std::size_t>(1 - m_toPlay)];
    const int level = m_level;
    const std::uint64_t seed = m_random();
    m_computer.Start([=](std::stop_token stop) { return nardy::Ai::Choose(*net, position, variant, roll, first, opponentFirst, level, seed, stop); });
}

void App::PlayStep(const Step& step)
{
    const nardy::Position after = nardy::Apply(MoverView(), step);
    m_view.Move(m_toPlay, ToPlace(step.from), ToPlace(step.to));
    SetFromMover(after);
    if (m_played.empty())
        m_lastMove.clear();
    m_played.push_back(step);
    m_view.UseDice(static_cast<int>(m_played.size()), m_roll.Double(), m_played.front().die == m_roll.first ? 0 : 1);
    m_hint.reset();
    m_hintWanted = false;
    m_hintWorker.Cancel();
    Select(-1);
    if (IsHuman(m_toPlay) && NextSteps().empty())
        m_doneClock = TurnPause;
}

void App::TakeBackStep()
{
    if (!HumanToMove() || m_played.empty())
        return;
    const Step step = m_played.back();
    m_played.pop_back();
    nardy::Position mover = m_toPlay == 0 ? m_turnStart : nardy::Flip(m_turnStart, GameVariant());
    for (const Step& played : m_played)
        mover = nardy::Apply(mover, played);
    SetFromMover(mover);
    // The checker flies back, and one it knocked off returns from the bar.
    m_view.Move(m_toPlay, ToPlace(step.to), ToPlace(step.from));
    if (step.hit)
        m_view.Move(1 - m_toPlay, BoardView::BarPlace, ToPlace(step.to));
    m_view.UseDice(static_cast<int>(m_played.size()), m_roll.Double(), m_played.empty() || m_played.front().die == m_roll.first ? 0 : 1);
    m_doneClock = -1;
    Select(-1);
}

void App::TakeBackTurn()
{
    while (HumanToMove() && !m_played.empty())
        TakeBackStep();
}

// Takes back this turn's steps, or else goes back to the start of the last turn a person played, with its dice.
void App::Undo()
{
    if (m_mode == Demo)
        return;
    if (HumanToMove() && !m_played.empty()) {
        TakeBackTurn();
        return;
    }
    while (!m_undo.empty() && m_undo.back().turn >= m_turn && m_stage != Stage::Over)
        m_undo.pop_back();
    if (m_undo.empty()) {
        ShowToast(std::string(T(Str::NothingToUndo)));
        return;
    }
    const Snapshot snapshot = m_undo.back();
    StopComputer();
    if (m_stage == Stage::Over && m_winner >= 0) {
        m_score[static_cast<std::size_t>(m_winner)] -= m_winPoints;
        m_bannerTime = 0;
    }
    m_winner = -1;
    m_board = snapshot.board;
    m_toPlay = snapshot.toPlay;
    m_firstMove = snapshot.firstMove;
    m_roll = snapshot.roll;
    m_turn = snapshot.turn;
    m_history.resize(std::min(m_history.size(), snapshot.history));
    m_lastMove.clear();
    m_rolled = true;
    m_view.Reset(m_board);
    m_view.ShowDice(m_toPlay, m_roll.first, m_roll.second);
    BeginMoves();
    Save();
}

void App::EndTurn()
{
    m_doneClock = -1;
    const int side = m_toPlay;
    const int high = std::max(m_roll.first, m_roll.second), low = std::min(m_roll.first, m_roll.second);
    const std::string moves = m_played.empty() ? std::string(T(Str::NoMoves)) : MoveText(m_played);
    AddHistory(std::format("{} {}-{}: {}", Name(side), high, low, moves), side);
    // Where each checker moved from and to: a checker going on with the next step marks only its start and its end.
    m_lastMove.clear();
    for (std::size_t i = 0; i < m_played.size(); ++i) {
        if (i == 0 || m_played[i].from != m_played[i - 1].to)
            m_lastMove.push_back(ToPlace(m_played[i].from));
        if (i + 1 == m_played.size() || m_played[i + 1].from != m_played[i].to)
            m_lastMove.push_back(ToPlace(m_played[i].to));
    }
    m_firstMove[static_cast<std::size_t>(side)] = false;
    if (m_board.off[static_cast<std::size_t>(side)] == nardy::Checkers) {
        FinishGame();
        return;
    }
    m_toPlay = 1 - side;
    StartTurn();
    Save();
}

void App::FinishGame()
{
    const int side = m_toPlay;
    m_winner = side;
    m_winPoints = nardy::WinPoints(MoverView(), GameVariant());
    m_score[static_cast<std::size_t>(side)] += m_winPoints;
    m_stage = Stage::Over;
    m_stageClock = 0;
    const std::string_view title = Pick(side, Str::WinYou, Str::WinComputer, Str::WinWhite, Str::WinBlack);
    const std::string_view points = T(m_winPoints >= 3 ? Str::PointsKoks : m_winPoints == 2 ? Str::PointsMars : Str::PointsOne);
    const std::string score = std::vformat(T(Str::Score), std::make_format_args(m_score[0], m_score[1]));
    ShowBanner(std::string(title), std::format("{}  ·  {}", points, score), -1);
    AddHistory(std::format("{} — {}", title, points), 3);
    m_sound.Play(SoundBank::Effect::Win);
    Save();
}

void App::StopComputer()
{
    m_computer.Cancel();
    m_computerMove.reset();
    m_hintWorker.Cancel();
    m_hintWanted = false;
}

void App::UpdateComputer(float seconds)
{
    if (m_stage == Stage::Thinking) {
        if (!m_computerMove)
            m_computerMove = m_computer.Take();
        if (m_computerMove && m_stageClock >= MinThink) {
            m_stage = Stage::Playing;
            m_stageClock = 0;
            m_computerStep = 0;
            m_stepClock = 0.1f;
        }
        return;
    }
    m_stepClock -= seconds;
    if (m_stepClock > 0 || !m_computerMove)
        return;
    if (m_computerStep < m_computerMove->count) {
        PlayStep(m_computerMove->steps[static_cast<std::size_t>(m_computerStep++)]);
        m_stepClock = m_computerStep < m_computerMove->count ? StepPause : TurnPause + 0.2f;
    }
    else {
        EndTurn();
    }
}

// The computer's advice for the whole roll, at its best: any steps already played are taken back first.
void App::RequestHint()
{
    if (!HumanToMove() || m_sequences.empty() || m_doneClock > 0)
        return;
    if (m_hint) {
        // Asked again: the move is played.
        const nardy::Move move = *m_hint;
        for (int i = 0; i < move.count; ++i)
            PlayStep(move.steps[static_cast<std::size_t>(i)]);
        return;
    }
    TakeBackTurn();
    m_hintWanted = true;
    const nardy::Network* net = &Net();
    const nardy::Position position = MoverView();
    const nardy::Variant variant = GameVariant();
    const nardy::Roll roll = m_roll;
    const bool first = m_firstMove[static_cast<std::size_t>(m_toPlay)], opponentFirst = m_firstMove[static_cast<std::size_t>(1 - m_toPlay)];
    const std::uint64_t seed = m_random();
    m_hintWorker.Start([=](std::stop_token stop) { return nardy::Ai::Choose(*net, position, variant, roll, first, opponentFirst, 5, seed, stop); });
}

void App::UpdateHint()
{
    if (!m_hintWanted)
        return;
    std::optional<nardy::Move> move = m_hintWorker.Take();
    if (!move)
        return;
    m_hintWanted = false;
    if (!HumanToMove() || !m_played.empty())
        return;
    m_hint = move;
    const std::vector<Step> steps(move->steps.begin(), move->steps.begin() + move->count);
    const std::string text = MoveText(steps);
    ShowToast(std::vformat(T(Str::HintText), std::make_format_args(text)));
}

std::vector<Step> App::NextSteps() const
{
    std::vector<Step> steps;
    const std::size_t k = m_played.size();
    for (const nardy::Move& move : m_sequences) {
        if (move.count <= static_cast<int>(k) || !StartsWith(move, m_played))
            continue;
        const Step& next = move.steps[k];
        if (std::ranges::find(steps, next) == steps.end())
            steps.push_back(next);
    }
    return steps;
}

// Where the checker on `point` may go this turn, by one step or by several of its own; a shorter way is preferred,
// and among equals one that hits on the way.
std::vector<App::Route> App::RoutesFrom(int point) const
{
    std::vector<Route> routes;
    const std::size_t k = m_played.size();
    auto hits = [](const std::vector<Step>& steps) { return std::ranges::count_if(steps, [](const Step& s) { return s.hit; }); };
    for (const nardy::Move& move : m_sequences) {
        if (move.count <= static_cast<int>(k) || move.steps[k].from != point || !StartsWith(move, m_played))
            continue;
        std::vector<Step> chain;
        for (std::size_t j = k; j < static_cast<std::size_t>(move.count); ++j) {
            const Step& step = move.steps[j];
            if (!chain.empty() && (step.from != chain.back().to || chain.back().to == nardy::BearOff))
                break;
            chain.push_back(step);
            const auto found = std::ranges::find_if(routes, [&](const Route& route) { return route.to == step.to; });
            if (found == routes.end())
                routes.push_back({step.to, chain});
            else if (chain.size() < found->steps.size() || (chain.size() == found->steps.size() && hits(chain) > hits(found->steps)))
                found->steps = chain;
        }
    }
    return routes;
}

void App::PlayRoute(const Route& route)
{
    for (const Step& step : route.steps)
        PlayStep(step);
}

// The checker on `point` goes by the higher die it may use, else the lower.
bool App::PlayLargerDie(int point)
{
    std::optional<Step> best;
    for (const Step& step : NextSteps())
        if (step.from == point && (!best || step.die > best->die))
            best = step;
    if (!best)
        return false;
    PlayStep(*best);
    return true;
}

void App::Select(int place)
{
    m_selected = place;
    if (place >= 0)
        m_view.Lift(m_toPlay, place);
    else
        m_view.Lift(-1, -1);
}

// Why a checker of the player to move may not go now, in words.
std::string App::WhyNot(int point) const
{
    const nardy::Position mover = MoverView();
    if (GameVariant() == nardy::Variant::Short && mover.bar[0] > 0 && point != nardy::FromBar)
        return std::string(T(Str::EnterFirst));
    if (GameVariant() == nardy::Variant::Long && point == nardy::Head && std::ranges::any_of(m_played, [](const Step& s) { return s.from == nardy::Head; }))
        return std::string(T(Str::HeadRule));
    return std::string(T(Str::CannotMove));
}

void App::SetGame(int game)
{
    m_game = (game + 2) % 2;
    m_score = {};
    NewGame();
}

void App::SetMode(int mode)
{
    mode = (mode + 3) % 3;
    if (mode == m_mode)
        return;
    const bool wasHuman = IsHuman(m_toPlay);
    m_mode = mode;
    m_score = {};
    // The player to move may have changed hands: their turn starts again for the new player.
    if (wasHuman != IsHuman(m_toPlay) && (m_stage == Stage::Moving || m_stage == Stage::Thinking || m_stage == Stage::Playing) && m_doneClock <= 0) {
        StopComputer();
        m_board = m_turnStart;
        m_view.Reset(m_board);
        BeginMoves();
    }
    Save();
}

void App::SetLevel(int level)
{
    m_level = std::clamp(level, 1, 5);
    Save();
}

void App::SetTheme(int theme)
{
    m_theme = (theme + ThemeCount) % ThemeCount;
    m_renderer->SetTheme(Themes()[static_cast<std::size_t>(m_theme)]);
    Save();
}

void App::SetCamera(int camera)
{
    m_camera.SetMode(static_cast<CameraMode>((camera + CameraModeCount) % CameraModeCount));
    Save();
}

void App::SetAutoRoll(bool on)
{
    m_autoRoll = on;
    m_stageClock = 0;
    Save();
}

void App::Save()
{
    m_settings.theme = m_theme;
    m_settings.level = m_level;
    m_settings.mode = m_mode;
    m_settings.game = m_game;
    m_settings.camera = static_cast<int>(m_camera.Mode());
    m_settings.autoRoll = m_autoRoll;
    m_settings.score = m_score;
    m_settings.sound = m_sound.enabled;
    m_settings.panel = m_panelVisible;
    m_settings.language = m_language;
    m_settings.match = SaveGame();
    m_settings.history.clear();
    if (!m_settings.match.empty()) {
        const std::size_t first = m_history.size() > 80 ? m_history.size() - 80 : 0;
        for (std::size_t i = first; i < m_history.size(); ++i)
            m_settings.history += std::format("{}{}{}", i > first ? "|" : "", m_history[i].kind, m_history[i].text);
    }
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (m_fullScreen)
        placement = m_placement;
    else if (m_window)
        GetWindowPlacement(m_window, &placement);
    if (placement.rcNormalPosition.right > placement.rcNormalPosition.left) {
        m_settings.hasWindow = true;
        m_settings.window = placement.rcNormalPosition;
        m_settings.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
    }
    m_settings.Save();
}

void App::ToggleFullScreen()
{
    const LONG style = GetWindowLongW(m_window, GWL_STYLE);
    if (!m_fullScreen) {
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        m_placement.length = sizeof(m_placement);
        if (GetWindowPlacement(m_window, &m_placement) && GetMonitorInfoW(MonitorFromWindow(m_window, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
            SetWindowLongW(m_window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            const RECT& r = monitor.rcMonitor;
            SetWindowPos(m_window, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            m_fullScreen = true;
        }
    }
    else {
        SetWindowLongW(m_window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(m_window, &m_placement);
        SetWindowPos(m_window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        m_fullScreen = false;
    }
}

void App::ToggleLanguage()
{
    m_language = m_language == Language::English ? Language::Russian : Language::English;
    SetWindowTextW(m_window, win32::Widen(T(Str::Title)).c_str());
    Save();
}

void App::AddHistory(std::string text, int kind)
{
    m_history.push_back({std::move(text), kind});
    m_followHistory = true;
}

// A turn as players write it, from the mover's side: one checker's steps joined ("24/13", or "24/18*/13" when it
// hit on the way), and the same move made twice counted ("13/7(2)").
std::string App::MoveText(const std::vector<Step>& steps) const
{
    auto name = [&](int point) {
        if (point == nardy::FromBar)
            return std::string(T(Str::Bar));
        if (point == nardy::BearOff)
            return std::string(T(Str::Off));
        return std::to_string(point + 1);
    };
    std::vector<std::string> parts;
    for (std::size_t i = 0; i < steps.size();) {
        std::size_t j = i;
        while (j + 1 < steps.size() && steps[j + 1].from == steps[j].to && steps[j].to != nardy::BearOff)
            ++j;
        std::string part = name(steps[i].from);
        for (std::size_t k = i; k <= j; ++k)
            if (k == j || steps[k].hit)
                part += "/" + name(steps[k].to) + (steps[k].hit ? "*" : "");
        parts.push_back(std::move(part));
        i = j + 1;
    }
    std::string text;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (std::find(parts.begin(), parts.begin() + static_cast<std::ptrdiff_t>(i), parts[i]) != parts.begin() + static_cast<std::ptrdiff_t>(i))
            continue;
        const auto times = std::count(parts.begin(), parts.end(), parts[i]);
        text += (text.empty() ? "" : " ") + parts[i] + (times > 1 ? std::format("({})", times) : "");
    }
    return text;
}

// Input --------------------------------------------------------------------------------------------

bool App::PointerRay(Vec3& origin, Vec3& direction) const
{
    if (m_pointer.x < 0 || m_pointer.y < 0)
        return false;
    m_camera.Ray(m_pointer.x, m_pointer.y, static_cast<float>(m_width), static_cast<float>(m_height), origin, direction);
    return true;
}

std::optional<Vec3> App::PointerOnPlane(float height) const
{
    Vec3 origin, direction;
    if (!PointerRay(origin, direction) || direction.y > -1e-4f)
        return std::nullopt;
    const float t = (height - origin.y) / direction.y;
    if (t < 0)
        return std::nullopt;
    return origin + direction * t;
}

int App::PlaceUnderPointer() const
{
    const std::optional<Vec3> hit = PointerOnPlane(board::CheckerHeight);
    if (!hit)
        return -1;
    const Vec2 xz{hit->x, hit->z};
    const int point = board::PointAt(xz);
    if (point >= 0)
        return point;
    if (std::abs(xz.x) < board::BarHalf && std::abs(xz.y) < board::HalfDepth)
        return BoardView::BarPlace;
    const float trayLeft = board::FieldRight + board::RimWidth * 0.5f;
    if (xz.x > trayLeft && xz.x < trayLeft + board::RimWidth + board::TrayWidth && std::abs(xz.y) < board::HalfDepth + board::RimWidth)
        return BoardView::OffPlace;
    return -1;
}

void App::HandleBoardPointer()
{
    const bool free = !m_pointer.consumed && !m_helpVisible;
    m_hoverPlace = free && m_gesture != Gesture::TurnCamera ? PlaceUnderPointer() : -1;
    const bool moving = HumanToMove() && m_doneClock <= 0;
    const std::vector<Step> next = moving ? NextSteps() : std::vector<Step>{};
    auto movable = [&](int place) {
        const int point = FromPlace(place);
        return std::ranges::any_of(next, [&](const Step& step) { return step.from == point; });
    };
    std::vector<Route> routes;
    if (moving && m_selected >= 0)
        routes = RoutesFrom(FromPlace(m_selected));
    auto routeTo = [&](int place) {
        return std::ranges::find_if(routes, [&](const Route& route) { return ToPlace(route.to) == place; });
    };
    const bool awaitingRoll = m_stage == Stage::ToRoll && IsHuman(m_toPlay) && !m_autoRoll;
    if (m_hoverPlace >= 0 && (movable(m_hoverPlace) || routeTo(m_hoverPlace) != routes.end()))
        m_cursorHand = true;
    if (free && awaitingRoll && m_hoverPlace >= 0)
        m_cursorHand = true;

    if (free && m_gesture == Gesture::None && (m_pointer.leftPressed || m_pointer.rightPressed)) {
        m_gesture = Gesture::Press;
        m_pressRight = !m_pointer.leftPressed;
        m_pressX = m_dragX = m_pointer.x;
        m_pressY = m_dragY = m_pointer.y;
        m_pressPlace = m_hoverPlace;
        m_pressDouble = m_pointer.doubleClick;
    }

    if (m_gesture == Gesture::Press) {
        const bool moved = std::hypot(m_pointer.x - m_pressX, m_pointer.y - m_pressY) > DragStart * Scale();
        const bool down = m_pressRight ? m_pointer.rightDown : m_pointer.leftDown;
        if (moved && down) {
            if (!m_pressRight && moving && movable(m_pressPlace)) {
                m_gesture = Gesture::Drag;
                Select(m_pressPlace);
            }
            else {
                m_gesture = Gesture::TurnCamera;
            }
        }
        else if (!down) {
            // A click.
            m_gesture = Gesture::None;
            const int place = m_pressPlace;
            if (m_pressRight) {
                if (HumanToMove() && !m_played.empty())
                    TakeBackStep();
                else
                    Select(-1);
            }
            else if (awaitingRoll) {
                if (place >= 0)
                    RollDice();
            }
            else if (moving) {
                const auto route = routeTo(place);
                const int toggled = std::exchange(m_toggledPlace, -1);
                if (m_pressDouble && toggled == place && movable(place)) {
                    PlayLargerDie(FromPlace(place));
                }
                else if (m_selected >= 0 && route != routes.end()) {
                    PlayRoute(*route);
                }
                else if (movable(place)) {
                    Select(place == m_selected ? -1 : place);
                    m_toggledPlace = place;
                }
                else {
                    const nardy::Position mover = MoverView();
                    const int point = FromPlace(place);
                    const bool own = point == nardy::FromBar ? mover.bar[0] > 0 : point >= 0 && point < nardy::Points && mover.points[static_cast<std::size_t>(point)] > 0;
                    if (own)
                        ShowToast(WhyNot(point));
                    Select(-1);
                }
            }
        }
    }

    if (m_gesture == Gesture::TurnCamera) {
        if (m_pressRight ? m_pointer.rightDown : m_pointer.leftDown) {
            m_camera.Rotate(-(m_pointer.x - m_dragX) * 0.006f, (m_pointer.y - m_dragY) * 0.005f);
            m_dragX = m_pointer.x;
            m_dragY = m_pointer.y;
        }
        else {
            m_gesture = Gesture::None;
        }
    }

    if (m_gesture == Gesture::Drag) {
        if (!moving || m_selected < 0) {
            m_gesture = Gesture::None;
            m_view.Carry(std::nullopt);
        }
        else if (m_pointer.leftDown) {
            if (const std::optional<Vec3> at = PointerOnPlane(0.03f))
                m_view.Carry(*at);
        }
        else {
            // Dropped: where it may go, else back to its place.
            m_gesture = Gesture::None;
            const auto route = routeTo(PlaceUnderPointer());
            if (route != routes.end()) {
                PlayRoute(*route);
            }
            else {
                m_view.Move(m_toPlay, m_selected, m_selected);
                Select(-1);
            }
        }
    }

    if (!m_pointer.consumed && !m_helpVisible && m_pointer.wheel != 0)
        m_camera.Zoom(m_pointer.wheel);
}

void App::HandleKey(WPARAM key)
{
    const bool control = GetKeyState(VK_CONTROL) < 0;
    const bool shift = GetKeyState(VK_SHIFT) < 0;

    if (m_helpVisible) {
        // Over the help only its own keys work, and full screen.
        if (key == VK_ESCAPE || key == VK_F1)
            m_helpVisible = false;
        else if (key == VK_F11)
            ToggleFullScreen();
        return;
    }
    if (control) {
        if (key == 'Z')
            Undo();
        else if (key == 'N')
            NewGame();
        return;
    }

    switch (key) {
    case VK_SPACE:
    case VK_RETURN:
        if (m_stage == Stage::ToRoll && IsHuman(m_toPlay))
            RollDice();
        else if (HumanToMove() && m_doneClock > 0)
            EndTurn();
        break;
    case 'H':
        RequestHint();
        break;
    case VK_BACK:
        if (HumanToMove() && !m_played.empty())
            TakeBackStep();
        else
            Undo();
        break;
    case 'A':
        SetAutoRoll(!m_autoRoll);
        break;
    case 'C':
        SetCamera(static_cast<int>(m_camera.Mode()) + 1);
        break;
    case VK_HOME:
        m_camera.Reset();
        break;
    case 'N':
        NewGame();
        break;
    case 'G':
        SetGame(m_game + 1);
        break;
    case 'M':
        SetMode(m_mode + (shift ? -1 : 1));
        break;
    case 'T':
        SetTheme(m_theme + (shift ? -1 : 1));
        break;
    case 'V':
        m_sound.enabled = !m_sound.enabled;
        Save();
        break;
    case 'L':
        ToggleLanguage();
        break;
    case VK_TAB:
        m_panelVisible = !m_panelVisible;
        Save();
        break;
    case VK_F1:
        m_helpVisible = !m_helpVisible;
        break;
    case VK_F11:
        ToggleFullScreen();
        break;
    case VK_ESCAPE:
        if (m_gesture == Gesture::Drag) {
            m_gesture = Gesture::None;
            m_view.Move(m_toPlay, m_selected, m_selected);
            Select(-1);
        }
        else if (m_selected >= 0) {
            Select(-1);
        }
        else if (m_fullScreen) {
            ToggleFullScreen();
        }
        break;
    default:
        if (key >= '1' && key <= '5')
            SetLevel(static_cast<int>(key - '0'));
        else if (key >= VK_NUMPAD1 && key <= VK_NUMPAD5)
            SetLevel(static_cast<int>(key - VK_NUMPAD0));
        break;
    }
}

// The interface ------------------------------------------------------------------------------------

Vec3 App::Accent() const
{
    return Themes()[static_cast<std::size_t>(m_theme)].accent;
}

Rect App::PanelRect() const
{
    const float scale = Scale();
    const float width = PanelWidth * scale, margin = PanelMargin * scale;
    return {static_cast<float>(m_width) - (width + margin) * m_panelSlide, margin, width, static_cast<float>(m_height) - 2 * margin};
}

Rect App::FreeArea() const
{
    const Rect panel = PanelRect();
    return {0, 0, std::min(panel.x, static_cast<float>(m_width)), static_cast<float>(m_height)};
}

namespace {

constexpr int HelpRows = 11; // in each of the two columns
constexpr float HelpWidth = 900, HelpRow = 26, HelpPad = 28;

} // namespace

std::vector<std::string> App::Wrap(std::string_view text, float width) const
{
    std::vector<std::string> lines;
    std::string line;
    while (!text.empty()) {
        const std::size_t space = std::min(text.find(' '), text.size());
        const std::string_view word = text.substr(0, space);
        text.remove_prefix(std::min(space + 1, text.size()));
        std::string longer = line.empty() ? std::string(word) : line + ' ' + std::string(word);
        if (!line.empty() && m_painter->Measure(FontId::Body, longer) > width) {
            lines.push_back(line);
            line = word;
        }
        else {
            line = std::move(longer);
        }
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

Rect App::HelpCard() const
{
    const float scale = Scale();
    const float width = std::min(HelpWidth * scale, FreeArea().width - 20 * scale);
    const Str rules = GameVariant() == nardy::Variant::Long ? Str::RulesLong : Str::RulesShort;
    const float rows = static_cast<float>(HelpRows + 1 + Wrap(T(rules), width - 2 * HelpPad * scale).size());
    const Rect free = FreeArea();
    const float height = (2 * HelpPad + 44 + 12) * scale + rows * HelpRow * scale;
    return {free.x + (free.width - width) / 2, std::max(10 * scale, (static_cast<float>(m_height) - height) / 2), width, height};
}

Rect App::BannerCard() const
{
    const float scale = Scale();
    const float width = std::max({m_painter->Measure(FontId::Display, m_bannerTitle) + 80 * scale, m_painter->Measure(FontId::Body, m_bannerDetail) + 60 * scale,
                                  320 * scale});
    const Rect free = FreeArea();
    return {free.x + (free.width - width) / 2, 26 * scale - (1 - m_bannerFade) * 20 * scale, width, 92 * scale};
}

std::vector<render::FrostedPanel> App::FrostedPanels() const
{
    std::vector<render::FrostedPanel> panels;
    const float scale = Scale();
    auto add = [&](Rect rect, float radius, float opacity) { panels.push_back({rect.x, rect.y, rect.width, rect.height, radius * scale, opacity}); };
    if (m_panelSlide > 0.001f)
        add(PanelRect(), 18, 1);
    if (m_bannerFade > 0.001f)
        add(BannerCard(), 18, m_bannerFade);
    if (m_helpVisible)
        add(HelpCard(), 20, 1);
    return panels;
}

void App::ShowToast(std::string text)
{
    if (m_toasts.size() >= 3)
        m_toasts.erase(m_toasts.begin());
    m_toasts.push_back({std::move(text), 0});
}

void App::ShowBanner(std::string title, std::string detail, float seconds)
{
    m_bannerTitle = std::move(title);
    m_bannerDetail = std::move(detail);
    m_bannerTime = seconds;
}

std::string_view App::Pick(int side, Str you, Str computer, Str white, Str black) const
{
    if (m_mode == VsComputer)
        return T(side == 0 ? you : computer);
    return T(side == 0 ? white : black);
}

std::string App::Name(int side) const
{
    return std::string(Pick(side, Str::NameYou, Str::NameComputer, Str::NameWhite, Str::NameBlack));
}

std::string App::StatusTitle() const
{
    if (m_stage == Stage::Opening)
        return std::string(T(Str::Opening));
    if (m_stage == Stage::Over)
        return std::string(Pick(m_winner, Str::WinYou, Str::WinComputer, Str::WinWhite, Str::WinBlack));
    return std::string(Pick(m_toPlay, Str::TurnYou, Str::TurnComputer, Str::TurnWhite, Str::TurnBlack));
}

std::string App::StatusDetail() const
{
    switch (m_stage) {
    case Stage::Opening:
        return std::string(T(Str::OpeningDetail));
    case Stage::Over:
        return std::string(T(m_mode == Demo ? Str::DemoNext : Str::PressNew));
    case Stage::ToRoll:
        return std::string(T(IsHuman(m_toPlay) && !m_autoRoll ? Str::RollPrompt : Str::Rolling));
    case Stage::Rolling:
        return std::string(T(Str::Rolling));
    case Stage::Thinking:
        return std::string(T(Str::Thinking));
    case Stage::Playing:
        return std::string(T(Str::ComputerMoving));
    case Stage::Moving:
        break;
    }
    if (m_sequences.empty())
        return std::string(T(Str::NoMovesToast));
    if (m_doneClock > 0)
        return std::string(T(Str::TurnDone));
    // The dice still to play.
    std::vector<int> dice = DiceOf(m_roll);
    for (const Step& step : m_played)
        if (const auto used = std::ranges::find(dice, step.die); used != dice.end())
            dice.erase(used);
    std::string left;
    for (const int die : dice)
        left += (left.empty() ? "" : ", ") + std::to_string(die);
    return std::vformat(T(Str::ToPlay), std::make_format_args(left));
}

bool App::Button(int id, Rect rect, std::string_view label, bool enabled, bool accent)
{
    const float scale = Scale();
    const bool modal = m_helpVisible;
    const bool interactive = enabled && (!modal || id > ModalWidgets);
    const bool hover = interactive && rect.Contains(m_pointer.x, m_pointer.y) && m_gesture == Gesture::None;
    if (hover)
        m_cursorHand = true;
    if (hover && m_pointer.leftPressed) {
        m_activeWidget = id;
        m_pointer.consumed = true;
    }
    bool clicked = false;
    if (m_pointer.leftReleased && m_activeWidget == id) {
        clicked = hover;
        m_activeWidget = 0;
        m_pointer.consumed = true;
    }
    const bool pressed = hover && m_activeWidget == id && m_pointer.leftDown;
    ui::Color fill = !enabled ? Rgba(255, 255, 255, 7) : pressed ? Rgba(255, 255, 255, 52) : hover ? Rgba(255, 255, 255, 34) : Rgba(255, 255, 255, 17);
    if (accent && enabled)
        fill = ui::Mix(fill, ui::FromLinear(Accent(), static_cast<float>(fill.a) / 255.0f * 2.2f), 0.6f);
    m_painter->Fill(rect, 9 * scale, fill);
    m_painter->TextCentered(FontId::Body, rect, label, enabled ? TextColor : ui::Fade(TextColor, 0.35f));
    if (clicked)
        m_sound.Play(SoundBank::Effect::Click);
    return clicked;
}

int App::Selector(int id, Rect rect, std::string_view label, std::string_view value)
{
    const float scale = Scale();
    m_painter->Text(FontId::Body, rect.x, rect.y + (rect.height - m_painter->LineHeight(FontId::Body)) / 2, label, MutedColor);
    const float arrow = rect.height;
    const Rect right{rect.Right() - arrow, rect.y, arrow, rect.height};
    const Rect left{rect.x + rect.width * 0.36f, rect.y, arrow, rect.height};
    m_painter->Fill({left.Right() + 2 * scale, rect.y, right.x - left.Right() - 4 * scale, rect.height}, 9 * scale, Rgba(255, 255, 255, 9));
    m_painter->TextCentered(FontId::Body, {left.Right(), rect.y, right.x - left.Right(), rect.height}, value, TextColor);
    int change = 0;
    if (Button(id, left, ""))
        change = -1;
    if (Button(id + 1, right, ""))
        change = 1;
    const float size = 5 * scale;
    m_painter->Arrow(left.x + left.width / 2, left.y + left.height / 2, size * 0.8f, size, -1, TextColor);
    m_painter->Arrow(right.x + right.width / 2, right.y + right.height / 2, size * 0.8f, size, 1, TextColor);
    return change;
}

void App::DrawInterface()
{
    DrawPanel();
    DrawBanner();
    DrawToasts();
    if (m_helpVisible)
        DrawHelp();
}

// A checker seen from above, in its side's colors from the theme.
void App::DrawChecker(float x, float y, float radius, int side)
{
    ui::Painter& p = *m_painter;
    const Theme& theme = Themes()[static_cast<std::size_t>(m_theme)];
    const Vec3 linear = theme.checkers[static_cast<std::size_t>(side)].color;
    // Brightened toward what the lit board shows.
    const ui::Color color = ui::FromLinear(linear * 1.25f + Vec3{0.02f, 0.02f, 0.02f});
    const bool light = linear.x + linear.y + linear.z > 0.6f;
    p.Disc(x, y + radius * 0.14f, radius * 1.06f, Rgba(0, 0, 0, 80));
    p.Disc(x, y, radius, ui::Mix(color, light ? Rgba(0, 0, 0) : Rgba(255, 255, 255), 0.18f));
    p.Disc(x, y, radius * 0.84f, color);
    p.Disc(x, y, radius * 0.5f, ui::Mix(color, light ? Rgba(0, 0, 0) : Rgba(255, 255, 255), 0.1f));
    p.Disc(x, y, radius * 0.42f, color);
}

// A die's face with its pips, darkened once played.
void App::DrawDie(float x, float y, float size, int value, float dim)
{
    ui::Painter& p = *m_painter;
    const Rect face{x - size / 2, y - size / 2, size, size};
    p.Shadow(face, size * 0.15f, Rgba(0, 0, 0, static_cast<int>(70 * (1 - dim * 0.6f))));
    p.Fill(face, size * 0.2f, ui::Mix(Rgba(240, 236, 226), Rgba(90, 92, 100), dim * 0.75f));
    static constexpr std::array<std::array<float, 2>, 9> spots{{{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
    static constexpr std::array<std::array<int, 6>, 6> layouts{{{4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8}}};
    const ui::Color pip = ui::Mix(Rgba(24, 22, 22), Rgba(60, 62, 70), dim);
    const auto& layout = layouts[static_cast<std::size_t>(std::clamp(value, 1, 6) - 1)];
    for (int i = 0; i < value; ++i) {
        const auto& spot = spots[static_cast<std::size_t>(layout[static_cast<std::size_t>(i)])];
        p.Disc(x + spot[0] * size * 0.27f, y + spot[1] * size * 0.27f, size * 0.095f, pip);
    }
}

void App::DrawStatus(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const ui::Color accent = ui::FromLinear(Accent());
    p.Fill(area, 12 * scale, Rgba(255, 255, 255, 14));

    const int side = m_stage == Stage::Over ? m_winner : m_toPlay;
    if (m_stage != Stage::Opening && side >= 0)
        DrawChecker(area.x + 24 * scale, area.y + area.height / 2, 11 * scale, side);

    // The dice of the turn on the right, the played ones darkened.
    float right = area.Right() - 10 * scale;
    const bool showDice = m_rolled && m_stage != Stage::Rolling && m_stage != Stage::ToRoll && m_stage != Stage::Over;
    if (showDice) {
        const std::vector<int> dice = DiceOf(m_roll);
        const float size = (dice.size() > 2 ? 17 : 22) * scale, gap = 5 * scale;
        std::vector<bool> used(dice.size(), false);
        for (const Step& step : m_played)
            for (std::size_t i = 0; i < dice.size(); ++i)
                if (!used[i] && dice[i] == step.die) {
                    used[i] = true;
                    break;
                }
        for (std::size_t i = dice.size(); i-- > 0;) {
            DrawDie(right - size / 2, area.y + area.height / 2, size, dice[i], used[i] ? 1.0f : 0.0f);
            right -= size + gap;
        }
        right -= 4 * scale;
    }
    if (m_stage == Stage::Thinking) {
        p.Spinner(right - 10 * scale, area.y + area.height / 2, 8 * scale, 2.2f * scale, m_time * 5.0f, accent);
        right -= 26 * scale;
    }

    const float textX = area.x + (m_stage == Stage::Opening ? 16 : 46) * scale;
    const float lineHeight = p.LineHeight(FontId::Heading);
    auto fit = [&](std::string text, FontId font) {
        const float room = right - textX;
        if (p.Measure(font, text) <= room)
            return text;
        // Cut to fit, at a whole character, with an ellipsis.
        while (text.size() > 1 && p.Measure(font, text + "…") > room) {
            std::size_t cut = text.size() - 1;
            while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut;
            text.resize(cut);
        }
        return text + "…";
    };
    p.Text(FontId::Heading, textX, area.y + area.height / 2 - lineHeight + 3 * scale, fit(StatusTitle(), FontId::Heading),
           m_stage == Stage::Over ? accent : TextColor);
    p.Text(FontId::Body, textX, area.y + area.height / 2 + 2 * scale, fit(StatusDetail(), FontId::Body), MutedColor);
}

// The players: who is to move, their pips and checkers borne off, and the points they have won.
void App::DrawPlayers(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const ui::Color accent = ui::FromLinear(Accent());
    const float rowHeight = 46 * scale;
    for (int side = 0; side < 2; ++side) {
        const float y = area.y + static_cast<float>(side) * rowHeight;
        const bool toMove = m_stage != Stage::Over && m_stage != Stage::Opening && m_toPlay == side;
        const bool winner = m_stage == Stage::Over && m_winner == side;
        if (toMove || winner)
            p.Fill({area.x - 6 * scale, y + 2 * scale, area.width + 12 * scale, rowHeight - 4 * scale}, 9 * scale, ui::Fade(accent, winner ? 0.3f : 0.16f));
        DrawChecker(area.x + 10 * scale, y + rowHeight / 2, 9 * scale, side);
        std::string name = Name(side);
        if (!IsHuman(side))
            name += std::format("  ·  {}", T(static_cast<Str>(std::to_underlying(Str::Level1) + m_level - 1)));
        const float textX = area.x + 28 * scale;
        const float bodyHeight = p.LineHeight(FontId::Body);
        p.Text(FontId::Body, textX, y + rowHeight / 2 - bodyHeight + 1 * scale, name, toMove || winner ? TextColor : ui::Fade(TextColor, 0.8f));
        const int pips = nardy::Pips(m_board, side, GameVariant());
        const int off = m_board.off[static_cast<std::size_t>(side)];
        p.Text(FontId::Body, textX, y + rowHeight / 2 + 1 * scale, std::vformat(T(Str::Pips), std::make_format_args(pips, off)), MutedColor);
        const std::string score = std::to_string(m_score[static_cast<std::size_t>(side)]);
        p.Text(FontId::Display, area.Right() - p.Measure(FontId::Display, score), y + (rowHeight - p.LineHeight(FontId::Display)) / 2, score,
               toMove || winner ? TextColor : ui::Fade(TextColor, 0.7f));
    }
}

void App::DrawHistory(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    p.Fill(area, 12 * scale, Rgba(0, 0, 0, 40));
    if (m_history.empty()) {
        p.TextCentered(FontId::Body, area, T(Str::NoTurnsYet), MutedColor);
        return;
    }
    const Rect inner = area.Inset(9 * scale);
    struct Line {
        std::string text;
        int kind;
        bool first;
    };
    std::vector<Line> lines;
    const std::size_t start = m_history.size() > 120 ? m_history.size() - 120 : 0;
    for (std::size_t i = start; i < m_history.size(); ++i) {
        bool first = true;
        for (std::string& line : Wrap(m_history[i].text, inner.width - 16 * scale)) {
            lines.push_back({std::move(line), m_history[i].kind, first});
            first = false;
        }
    }
    const float rowHeight = std::round(22 * scale);
    const int visible = std::max(1, static_cast<int>(inner.height / rowHeight));
    const float maxScroll = static_cast<float>(std::max(0, static_cast<int>(lines.size()) - visible));
    if (area.Contains(m_pointer.x, m_pointer.y) && m_pointer.wheel != 0 && !m_helpVisible) {
        m_historyScroll = std::clamp(m_historyScroll - m_pointer.wheel * 3, 0.0f, maxScroll);
        m_followHistory = m_historyScroll >= maxScroll;
        m_pointer.wheel = 0;
    }
    if (m_followHistory)
        m_historyScroll = maxScroll;
    m_historyScroll = std::clamp(m_historyScroll, 0.0f, maxScroll);
    const ui::Color accent = ui::FromLinear(Accent());
    const Theme& theme = Themes()[static_cast<std::size_t>(m_theme)];
    const int first = static_cast<int>(m_historyScroll);
    for (int i = first; i < std::min(static_cast<int>(lines.size()), first + visible); ++i) {
        const Line& line = lines[static_cast<std::size_t>(i)];
        const float y = inner.y + static_cast<float>(i - first) * rowHeight;
        const ui::Color color = line.kind == 2 ? NoteColor : line.kind == 3 ? accent : TextColor;
        if (line.first) {
            if (line.kind <= 1)
                p.Disc(inner.x + 4 * scale, y + rowHeight / 2, 3.5f * scale,
                       ui::FromLinear(theme.checkers[static_cast<std::size_t>(line.kind)].color * 1.25f + Vec3{0.03f, 0.03f, 0.03f}));
            else
                p.Disc(inner.x + 4 * scale, y + rowHeight / 2, 2.5f * scale, ui::Fade(color, 0.7f));
        }
        p.Text(FontId::Body, inner.x + 16 * scale, y + (rowHeight - p.LineHeight(FontId::Body)) / 2, line.text,
               i == static_cast<int>(lines.size()) - 1 || line.kind >= 2 ? color : ui::Fade(color, 0.82f));
    }
    if (maxScroll > 0) {
        const float barHeight = inner.height * static_cast<float>(visible) / static_cast<float>(lines.size());
        const float barY = inner.y + (inner.height - barHeight) * (m_historyScroll / maxScroll);
        p.Fill({area.Right() - 5 * scale, barY, 3 * scale, barHeight}, 1.5f * scale, Rgba(255, 255, 255, 50));
    }
}

void App::DrawPanel()
{
    if (m_panelSlide <= 0.001f)
        return;
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect panel = PanelRect();
    if (panel.Contains(m_pointer.x, m_pointer.y) && (m_pointer.leftPressed || m_pointer.rightPressed) && m_gesture == Gesture::None)
        m_pointer.consumed = true;
    p.Shadow(panel, 22 * scale, Rgba(0, 0, 0, 70));
    p.Fill(panel, 18 * scale, Rgba(16, 18, 26, 120));
    p.Outline(panel, 18 * scale, Rgba(255, 255, 255, 24));

    // In a low window everything is packed closer, and the footer goes.
    const bool compact = panel.height < 820 * scale;
    const float pad = (compact ? 16 : 20) * scale;
    const float x = panel.x + pad, width = panel.width - 2 * pad;
    float y = panel.y + pad;

    p.Text(FontId::Heading, x, y, T(Str::Title), TextColor);
    if (Button(HelpButton, {panel.Right() - pad - 30 * scale, y - 2 * scale, 30 * scale, 30 * scale}, "?"))
        m_helpVisible = !m_helpVisible;
    y += (compact ? 38 : 44) * scale;

    const float statusHeight = (compact ? 54 : 62) * scale;
    DrawStatus({x, y, width, statusHeight});
    y += statusHeight + (compact ? 6 : 10) * scale;

    const float playersHeight = 92 * scale;
    DrawPlayers({x + 4 * scale, y, width - 8 * scale, playersHeight});
    y += playersHeight + (compact ? 6 : 10) * scale;

    // The buttons of the turn.
    const float rowHeight = (compact ? 29 : 34) * scale, gap = (compact ? 5 : 8) * scale;
    const float half = (width - gap) / 2;
    const float buttonHeight = (compact ? 31 : 38) * scale;
    const bool canRoll = m_stage == Stage::ToRoll && IsHuman(m_toPlay);
    if (Button(RollButton, {x, y, half, buttonHeight}, T(Str::Roll), canRoll, canRoll))
        RollDice();
    const bool canHint = HumanToMove() && !m_sequences.empty() && m_doneClock <= 0 && !m_hintWanted;
    if (Button(HintButton, {x + half + gap, y, half, buttonHeight}, T(Str::Hint), canHint, m_hint.has_value() && canHint))
        RequestHint();
    y += buttonHeight + gap;
    const bool earlier = std::ranges::any_of(m_undo, [&](const Snapshot& snapshot) { return snapshot.turn < m_turn || m_stage == Stage::Over; });
    const bool canUndo = m_mode != Demo && ((HumanToMove() && !m_played.empty()) || earlier);
    if (Button(UndoButton, {x, y, half, buttonHeight}, T(Str::Undo), canUndo))
        Undo();
    if (Button(NewGameButton, {x + half + gap, y, half, buttonHeight}, T(Str::NewGame), true, m_stage == Stage::Over))
        NewGame();
    y += buttonHeight + (compact ? 8 : 12) * scale;

    // From the bottom up: the footer, the switches and the settings; the moves take the space between.
    float bottom = panel.Bottom() - pad;
    if (!compact) {
        const float footerHeight = p.LineHeight(FontId::Body);
        p.TextCentered(FontId::Body, {x, bottom - footerHeight, width, footerHeight}, T(Str::Footer), ui::Fade(MutedColor, 0.8f));
        bottom -= footerHeight + 14 * scale;
    }

    bottom -= rowHeight;
    if (Button(SoundButton, {x, bottom, half, rowHeight}, T(Str::Sound), true, m_sound.enabled)) {
        m_sound.enabled = !m_sound.enabled;
        Save();
    }
    if (Button(LanguageButton, {x + half + gap, bottom, half, rowHeight}, T(Str::LanguageName)))
        ToggleLanguage();
    bottom -= gap + rowHeight;
    if (Button(AutoRollButton, {x, bottom, half, rowHeight}, T(Str::AutoRoll), true, m_autoRoll))
        SetAutoRoll(!m_autoRoll);
    static constexpr std::array<Str, CameraModeCount> cameras{Str::CameraOverview, Str::CameraTop};
    const std::string view = std::format("{}: {}", T(Str::Camera), T(cameras[static_cast<std::size_t>(m_camera.Mode())]));
    if (Button(CameraButton, {x + half + gap, bottom, half, rowHeight}, view))
        SetCamera(static_cast<int>(m_camera.Mode()) + 1);
    bottom -= gap;

    static constexpr std::array<Str, 3> modes{Str::ModeComputer, Str::ModeTwoPlayers, Str::ModeDemo};
    static constexpr std::array<Str, 2> games{Str::GameLong, Str::GameShort};
    const Theme& theme = Themes()[static_cast<std::size_t>(m_theme)];
    struct Choice {
        int id;
        Str label;
        std::string value;
    };
    const std::array<Choice, 4> choices{{
        {ThemeSelector, Str::Theme, std::string(m_language == Language::Russian ? theme.nameRussian : theme.nameEnglish)},
        {LevelSelector, Str::Level, std::format("{} · {}", m_level, T(static_cast<Str>(std::to_underlying(Str::Level1) + m_level - 1)))},
        {ModeSelector, Str::Mode, std::string(T(modes[static_cast<std::size_t>(m_mode)]))},
        {GameSelector, Str::Game, std::string(T(games[static_cast<std::size_t>(m_game)]))},
    }};
    for (const Choice& choice : choices) {
        bottom -= rowHeight;
        if (const int change = Selector(choice.id, {x, bottom, width, rowHeight}, T(choice.label), choice.value); change != 0) {
            switch (choice.id) {
            case ThemeSelector:
                SetTheme(m_theme + change);
                break;
            case LevelSelector:
                SetLevel(m_level + change);
                break;
            case ModeSelector:
                SetMode(m_mode + change);
                break;
            default:
                SetGame(m_game + change);
                break;
            }
        }
        bottom -= (compact ? 3 : 6) * scale;
    }
    bottom -= (compact ? 6 : 10) * scale;

    // The moves so far, where there is room for them.
    const float labelHeight = p.LineHeight(FontId::Body) + 6 * scale;
    if (bottom - y - labelHeight > 36 * scale) {
        p.Text(FontId::Body, x, y, T(Str::History), MutedColor);
        DrawHistory({x, y + labelHeight, width, bottom - y - labelHeight});
    }
}

void App::DrawBanner()
{
    if (m_bannerFade <= 0.001f)
        return;
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect card = BannerCard();
    p.Shadow(card, 24 * scale, ui::Fade(Rgba(0, 0, 0, 120), m_bannerFade));
    p.Fill(card, 18 * scale, ui::Fade(Rgba(14, 16, 24, 150), m_bannerFade));
    p.Outline(card, 18 * scale, ui::Fade(ui::FromLinear(Accent()), 0.6f * m_bannerFade));
    p.TextCentered(FontId::Display, {card.x, card.y + 8 * scale, card.width, 48 * scale}, m_bannerTitle, ui::Fade(TextColor, m_bannerFade));
    p.TextCentered(FontId::Body, {card.x, card.y + 54 * scale, card.width, 26 * scale}, m_bannerDetail, ui::Fade(MutedColor, m_bannerFade));
}

void App::DrawHelp()
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    p.Fill({0, 0, static_cast<float>(m_width), static_cast<float>(m_height)}, 0, Rgba(0, 0, 0, 110));

    struct Line {
        std::string_view keys;
        Str action;
    };
    const std::array<Line, 2 * HelpRows - 1> lines{{
        {T(Str::KeyClick), Str::HelpClick},
        {T(Str::KeyDrag), Str::HelpDrag},
        {T(Str::KeyDoubleClick), Str::HelpDoubleClick},
        {T(Str::KeyRightClick), Str::HelpTakeBack},
        {T(Str::KeyRightDrag), Str::HelpRotate},
        {T(Str::KeyWheel), Str::HelpZoom},
        {"Space  Enter", Str::HelpRoll},
        {"H", Str::HelpHint},
        {"Ctrl+Z", Str::HelpUndo},
        {"A", Str::HelpAutoRoll},
        {"C  Home", Str::HelpCamera},
        {"N", Str::HelpNew},
        {"G", Str::HelpGame},
        {"M", Str::HelpMode},
        {"1 – 5", Str::HelpLevel},
        {"T", Str::HelpTheme},
        {"V", Str::HelpSound},
        {"L", Str::HelpLanguage},
        {"Tab", Str::HelpPanel},
        {"F11  Alt+Enter", Str::HelpFullScreen},
        {"Esc  F1", Str::HelpClose},
    }};
    const float rowHeight = HelpRow * scale, pad = HelpPad * scale;
    const Rect card = HelpCard();
    const ui::Color accent = ui::FromLinear(Accent());
    p.Shadow(card, 26 * scale, Rgba(0, 0, 0, 140));
    p.Fill(card, 20 * scale, Rgba(18, 20, 28, 175));
    p.Outline(card, 20 * scale, Rgba(255, 255, 255, 30));
    p.Text(FontId::Heading, card.x + pad, card.y + pad - 4 * scale, T(Str::Controls), TextColor);
    const float top = card.y + pad + 44 * scale;
    const float leftWidth = (card.width - 2 * pad) * 0.57f;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const bool second = i >= static_cast<std::size_t>(HelpRows - 1);
        const float columnX = card.x + pad + (second ? leftWidth : 0.0f);
        const float y = top + static_cast<float>(second ? i - (HelpRows - 1) : i) * rowHeight;
        const float keysWidth = (second ? 130 : 228) * scale;
        p.Text(FontId::Body, columnX, y, lines[i].keys, accent);
        p.Text(FontId::Body, columnX + keysWidth, y, T(lines[i].action), TextColor);
    }
    // The rules of the game on the board, in a few sentences.
    float y = top + static_cast<float>(HelpRows) * rowHeight + 12 * scale;
    p.Text(FontId::Body, card.x + pad, y, T(Str::Rules), accent);
    for (const std::string& line : Wrap(T(GameVariant() == nardy::Variant::Long ? Str::RulesLong : Str::RulesShort), card.width - 2 * pad)) {
        y += rowHeight;
        p.Text(FontId::Body, card.x + pad, y, line, MutedColor);
    }
    if (Button(HelpClose, {card.Right() - pad - 30 * scale, card.y + pad - 8 * scale, 30 * scale, 30 * scale}, "×") ||
        (m_pointer.leftPressed && !card.Contains(m_pointer.x, m_pointer.y)))
        m_helpVisible = false;
    if (m_pointer.leftPressed || m_pointer.rightPressed)
        m_pointer.consumed = true;
}

void App::DrawToasts()
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect free = FreeArea();
    float y = static_cast<float>(m_height) - 40 * scale;
    for (auto toast = m_toasts.rbegin(); toast != m_toasts.rend(); ++toast) {
        const float fade = std::min({1.0f, toast->age * 6, (2.8f - toast->age) * 3});
        const float width = p.Measure(FontId::Body, toast->text) + 40 * scale, height = 36 * scale;
        const Rect pill{free.x + (free.width - width) / 2, y - height, width, height};
        p.Shadow(pill, 14 * scale, ui::Fade(Rgba(0, 0, 0, 110), fade));
        p.Fill(pill, height / 2, ui::Fade(Rgba(20, 22, 30, 225), fade));
        p.TextCentered(FontId::Body, pill, toast->text, ui::Fade(TextColor, fade));
        y -= height + 8 * scale;
    }
}
