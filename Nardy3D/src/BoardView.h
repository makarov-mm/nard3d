#pragma once

#include "MathTypes.h"
#include "Nardy.h"

#include <array>
#include <optional>
#include <random>
#include <vector>

// How the checkers and the dice are shown: every checker in its place, flying in an arc when it moves, lifted when
// chosen or dragged; and the dice thrown onto the board, tumbling and coming to rest on the numbers rolled.
class BoardView {
public:
    // A place for checkers: a point (as White counts them), the bar, or the tray.
    static constexpr int BarPlace = 24, OffPlace = 25, Places = 26;

    struct Piece {
        math::Mat4 model;
        int kind = 0; // 0 a checker, 1 a die
        int side = 0; // White or Black
        int value = 0;
        float highlight = 0;
        float dim = 0; // a die already played
    };

    // What was heard since the last call: checkers landing, knocking a lone one off or dropping into the tray, and
    // the dice striking the board, each as loud as it strikes.
    struct Sounds {
        int landings = 0, hits = 0, trays = 0;
        std::vector<float> dice;
    };

    // Every checker in its place at once; `board` is seen from White's side.
    void Reset(const nardy::Position& board);
    // The top checker of `side` at `from` goes to `to`, after `delay` seconds; a lone checker hit there goes to the bar.
    void Move(int side, int from, int to, float delay = 0);
    // A checker raised over its place, as chosen, or carried by the pointer.
    void Lift(int side, int place);
    void Carry(std::optional<math::Vec3> position);
    // Throws dice: one each for the two players at the start, else two by `side`.
    void Throw(int side, int first, int second, std::mt19937_64& random);
    void ThrowOpening(int white, int black, std::mt19937_64& random);
    // Two dice of `side` lying still on the numbers given, as a turn taken back left them.
    void ShowDice(int side, int first, int second);
    void ClearDice() { m_dice = {}; }
    // How many of the roll's moves are played: the dice darken as they are used.
    void UseDice(int used, bool doubles, int firstUsed);

    void Update(float seconds);
    bool Animating() const;
    bool DiceRolling() const;
    Sounds TakeSounds();
    // The checkers and the dice to draw; the top checkers of `glowSide` glow as `glow` says for their places.
    void AddPieces(std::vector<Piece>& pieces, const std::array<float, Places>& glow, int glowSide) const;

private:
    struct Checker {
        int side = 0;
        int place = 0;
        math::Vec3 from, to; // the flight
        float time = 1, duration = 1, delay = 0;
        float lift = 0;      // raised when chosen
        float standFrom = 0; // 1 if it starts its flight standing in the tray
        bool hitOnArrival = false;
    };
    struct Die {
        bool visible = false;
        int side = 0, value = 1;
        math::Vec3 from, to;
        math::Vec4 rest; // quaternion it comes to rest in
        math::Vec3 spinAxis;
        float spin = 0;
        float time = 1, duration = 1;
        float dim = 0, dimTarget = 0;
        int bounces = 0;
    };

    math::Vec3 Spot(int side, int place, int index) const;
    // Where the top checker of a place is drawn now, in flight or not.
    math::Vec3 TopOf(int side, int place) const;
    math::Mat4 CheckerModel(const Checker& checker) const;
    void ThrowDie(Die& die, int side, int index, int value, math::Vec3 from, std::mt19937_64& random);

    std::array<Checker, 30> m_checkers{};
    std::array<std::array<std::vector<int>, Places>, 2> m_stacks{}; // checker ids at each place, bottom first
    std::array<Die, 2> m_dice{};
    int m_liftedSide = -1, m_liftedPlace = -1;
    std::optional<math::Vec3> m_carried;
    Sounds m_sounds;
};
