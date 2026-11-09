#pragma once

#include <array>
#include <cstdint>
#include <vector>

// The rules of the two games of nardy. In long nardy both sides run the same way round the board, a single checker
// holds a point, only one checker a turn may leave the head, and no six points in a row may be held while they shut
// the opponent in. In short nardy (backgammon) the sides run against each other, and a lone checker is hit to the
// bar and must enter again.
namespace nardy {

enum class Variant : std::uint8_t { Long, Short };

constexpr int Points = 24;
constexpr int Checkers = 15;
constexpr int Head = 23;    // where long nardy starts all fifteen
constexpr int FromBar = 24; // a step entering from the bar
constexpr int BearOff = -1; // a step bearing a checker off

// A position as the player to move sees it: points[i] above zero are their checkers, below zero the opponent's. The
// player to move brings their checkers from point 23 down to point 0, their home is points 0 to 5, and they bear off
// below point 0. Points are numbered for people from 1, as the mover sees them.
struct Position {
    std::array<std::int8_t, Points> points{};
    std::array<std::int8_t, 2> bar{}; // the mover's, then the opponent's: short nardy only
    std::array<std::int8_t, 2> off{}; // borne off

    friend bool operator==(const Position&, const Position&) = default;
};

struct Step {
    std::int8_t from = 0; // a point, or FromBar
    std::int8_t to = 0;   // a point, or BearOff
    std::int8_t die = 0;
    bool hit = false;

    friend bool operator==(const Step&, const Step&) = default;
};

// A whole turn: up to four steps, and where they leave the board, still from the mover's side.
struct Move {
    std::array<Step, 4> steps{};
    int count = 0;
    Position after;
};

struct Roll {
    int first = 1, second = 1;
    bool Double() const { return first == second; }
};

Position Start(Variant variant);
// The same position seen by the other player.
Position Flip(const Position& position, Variant variant);
Position Apply(const Position& position, const Step& step);
// The pips the mover (side 0) or the opponent (side 1) still has to travel.
int Pips(const Position& position, int side, Variant variant);
// Long nardy: the mover holds no six points in a row unless an opponent's checker has already passed them.
bool BlockAllowed(const Position& position);

// The ways to play a roll, each final position once, with one order of steps that reaches it. As many dice as
// possible must be played, and the higher one when only one can be. `firstMove` lets two checkers leave the head of
// long nardy on a first roll of 6-6, 4-4 or 3-3. Empty when nothing can be played.
std::vector<Move> LegalMoves(const Position& position, Variant variant, Roll roll, bool firstMove);
// Every order of steps that plays the roll legally, for a player choosing step by step.
std::vector<Move> LegalSequences(const Position& position, Variant variant, Roll roll, bool firstMove);

// After the mover has borne off their last checker: 1, 2 for a gammon (марс), or 3 for a backgammon.
int WinPoints(const Position& position, Variant variant);

} // namespace nardy
