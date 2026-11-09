#pragma once

#include "Ai.h"
#include "BoardView.h"
#include "Camera.h"
#include "Gl.h"
#include "Nardy.h"
#include "Network.h"
#include "Renderer.h"
#include "Settings.h"
#include "Sound.h"
#include "Strings.h"
#include "Text.h"
#include "UiPainter.h"
#include "Win32.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// The game's window: it runs the games, the computer players and the interface, and draws them.
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run();

private:
    enum Mode { VsComputer, TwoPlayers, Demo };

    // The players throw a die each to see who starts; then each turn the dice are thrown and fly, and the checkers
    // are moved: by hand, or by the computer, which thinks and then plays its steps one by one. Then the game is over.
    enum class Stage { Opening, ToRoll, Rolling, Moving, Thinking, Playing, Over };

    // What the mouse did since the last frame.
    struct Pointer {
        float x = -1, y = -1;
        bool leftDown = false, rightDown = false;
        bool leftPressed = false, leftReleased = false, rightPressed = false, rightReleased = false;
        bool doubleClick = false;
        float wheel = 0;
        bool consumed = false; // taken by the interface
    };

    enum class Gesture { None, Press, Drag, TurnCamera };

    struct Toast {
        std::string text;
        float age = 0;
    };

    // A human's turn as it began, with its dice: what taking back a turn returns to.
    struct Snapshot {
        nardy::Position board;
        int toPlay = 0;
        std::array<bool, 2> firstMove{};
        nardy::Roll roll;
        std::size_t history = 0;
        int turn = 0;
    };

    struct HistoryLine {
        std::string text;
        int kind = 0; // 0 White's turn, 1 Black's, 2 a note, 3 the end of the game
    };

    // A way for the checker chosen to reach a place: the steps, one checker's, in order.
    struct Route {
        int to = 0; // in the mover's numbers: a point or BearOff
        std::vector<nardy::Step> steps;
    };

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateMainWindow();
    void ShowLoading();
    void BuildFonts();
    void LoadNetworks();
    void Frame();
    void Update(float seconds);
    void PlaySounds();
    bool NeedsAnimation() const;
    render::Scene BuildScene() const;

    // The game.
    nardy::Variant GameVariant() const { return m_game == 0 ? nardy::Variant::Long : nardy::Variant::Short; }
    const nardy::Network& Net() const { return m_nets[static_cast<std::size_t>(m_game)]; }
    bool IsHuman(int side) const;
    bool HumanToMove() const { return m_stage == Stage::Moving && IsHuman(m_toPlay); }
    // The board as the player to move sees it, and back.
    nardy::Position MoverView() const;
    void SetFromMover(const nardy::Position& mover);
    // A place in the mover's numbers (a point, FromBar or BearOff) as BoardView counts places, and back.
    int ToPlace(int point) const;
    int FromPlace(int place) const;
    bool LoadGame(const std::string& text);
    std::string SaveGame() const;
    void NewGame();
    void ThrowOpening();
    void FinishOpening();
    void StartTurn();
    void RollDice();
    void BeginMoves();
    void PlayStep(const nardy::Step& step);
    void TakeBackStep();
    void TakeBackTurn();
    void Undo();
    void EndTurn();
    void FinishGame();
    void StopComputer();
    void UpdateComputer(float seconds);
    void RequestHint();
    void UpdateHint();
    // The steps that may be played next, and the routes from a place in the mover's numbers.
    std::vector<nardy::Step> NextSteps() const;
    std::vector<Route> RoutesFrom(int point) const;
    void PlayRoute(const Route& route);
    bool PlayLargerDie(int point);
    void Select(int place);
    std::string WhyNot(int point) const;
    void SetGame(int game);
    void SetMode(int mode);
    void SetLevel(int level);
    void SetTheme(int theme);
    void SetCamera(int camera);
    void SetAutoRoll(bool on);
    void Save();
    void ToggleFullScreen();
    void ToggleLanguage();
    void AddHistory(std::string text, int kind);
    std::string MoveText(const std::vector<nardy::Step>& steps) const;

    // Input.
    void HandleBoardPointer();
    void HandleKey(WPARAM key);
    bool PointerRay(math::Vec3& origin, math::Vec3& direction) const;
    std::optional<math::Vec3> PointerOnPlane(float height) const;
    // The place under the pointer, as BoardView counts places, or -1.
    int PlaceUnderPointer() const;

    // The interface.
    float Scale() const { return static_cast<float>(m_dpi) / 96.0f; }
    ui::Rect PanelRect() const;
    ui::Rect FreeArea() const;
    std::vector<render::FrostedPanel> FrostedPanels() const;
    std::vector<std::string> Wrap(std::string_view text, float width) const;
    ui::Rect HelpCard() const;
    ui::Rect BannerCard() const;
    void DrawInterface();
    void DrawPanel();
    void DrawStatus(ui::Rect area);
    void DrawPlayers(ui::Rect area);
    void DrawHistory(ui::Rect area);
    void DrawBanner();
    void DrawHelp();
    void DrawToasts();
    void DrawChecker(float x, float y, float radius, int side);
    void DrawDie(float x, float y, float size, int value, float dim);
    bool Button(int id, ui::Rect rect, std::string_view label, bool enabled = true, bool accent = false);
    // A label with a value stepped by two arrows; returns -1 or 1 when one is clicked, else 0.
    int Selector(int id, ui::Rect rect, std::string_view label, std::string_view value);
    void ShowToast(std::string text);
    void ShowBanner(std::string title, std::string detail, float seconds);
    std::string_view T(Str id) const { return Tr(id, m_language); }
    // The words for one side: "you" or "the computer" against the computer, else "White" or "Black".
    std::string_view Pick(int side, Str you, Str computer, Str white, Str black) const;
    std::string Name(int side) const;
    std::string StatusTitle() const;
    std::string StatusDetail() const;
    math::Vec3 Accent() const;

    HINSTANCE m_instance;
    HWND m_window = nullptr;
    std::unique_ptr<gl::Context> m_context;
    std::unique_ptr<render::Renderer> m_renderer;
    std::unique_ptr<ui::Painter> m_painter;
    std::array<std::unique_ptr<text::Font>, 3> m_fonts;
    SoundBank m_sound;
    Settings m_settings;
    Language m_language = Language::English;
    int m_width = 0, m_height = 0;
    UINT m_dpi = 96;
    UINT m_fontDpi = 0;
    bool m_running = true;
    bool m_dirty = true; // something to draw since the last frame
    bool m_fullScreen = false;
    WINDOWPLACEMENT m_placement{sizeof(WINDOWPLACEMENT)};
    std::chrono::steady_clock::time_point m_lastFrame;
    float m_time = 0;

    // The game.
    std::array<nardy::Network, 2> m_nets; // long and short nardy
    nardy::Position m_board;              // as White sees it
    int m_toPlay = 0;
    std::array<bool, 2> m_firstMove{true, true};
    nardy::Roll m_roll;
    bool m_rolled = false;
    std::array<int, 2> m_opening{}; // the dice thrown to see who starts
    nardy::Position m_turnStart;
    std::vector<nardy::Step> m_played;    // this turn's steps so far, in the mover's numbers
    std::vector<nardy::Move> m_sequences; // every legal way to play this turn's roll
    std::vector<int> m_lastMove;          // the places the last turn moved from and to, as BoardView counts them
    std::array<int, 2> m_score{};
    int m_winner = -1;
    int m_winPoints = 0;
    Stage m_stage = Stage::Opening;
    float m_stageClock = 0;
    float m_doneClock = -1; // counts down after the last step of a turn, before it ends
    int m_mode = VsComputer;
    int m_level = 3;
    int m_theme = 0;
    int m_game = 0;
    bool m_autoRoll = true;
    int m_turn = 0;
    std::mt19937_64 m_random;
    std::vector<Snapshot> m_undo;
    std::vector<HistoryLine> m_history;
    BoardView m_view;

    // The computer.
    nardy::Worker<nardy::Move> m_computer;
    std::optional<nardy::Move> m_computerMove;
    int m_computerStep = 0;
    float m_stepClock = 0;
    nardy::Worker<nardy::Move> m_hintWorker;
    bool m_hintWanted = false;
    std::optional<nardy::Move> m_hint;

    // The view and the input.
    BoardCamera m_camera;
    Pointer m_pointer;
    Gesture m_gesture = Gesture::None;
    float m_pressX = 0, m_pressY = 0;
    float m_dragX = 0, m_dragY = 0;
    bool m_pressRight = false;
    bool m_pressDouble = false;
    int m_pressPlace = -1;
    int m_toggledPlace = -1; // where the last click chose or let go of a checker, for a double-click
    int m_selected = -1; // the place chosen to move from, as BoardView counts places
    int m_hoverPlace = -1;
    bool m_cursorHand = false;
    bool m_trackingLeave = false;

    // The interface.
    int m_activeWidget = 0;
    bool m_panelVisible = true;
    float m_panelSlide = 1;
    bool m_helpVisible = false;
    float m_historyScroll = 0;
    bool m_followHistory = true;
    float m_bannerFade = 0;
    float m_bannerTime = 0; // left to show; below zero it stays
    std::string m_bannerTitle, m_bannerDetail;
    std::vector<Toast> m_toasts;
};
