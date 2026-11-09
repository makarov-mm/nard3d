#include "Nardy.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace nardy {

namespace {

// Keys met in one search, cleared at once by moving on to a new epoch.
class SeenSet {
public:
    void Clear()
    {
        if (++m_epoch == 0) {
            std::ranges::fill(m_epochs, 0u);
            m_epoch = 1;
        }
    }

    // False when the key was already there. A crowded table lets keys through rather than look further.
    bool Insert(std::uint64_t key)
    {
        std::size_t slot = static_cast<std::size_t>(key ^ (key >> 29)) & (Size - 1);
        for (int probe = 0; probe < 32; ++probe, slot = (slot + 1) & (Size - 1)) {
            if (m_epochs[slot] != m_epoch) {
                m_epochs[slot] = m_epoch;
                m_keys[slot] = key;
                return true;
            }
            if (m_keys[slot] == key)
                return false;
        }
        return true;
    }

private:
    static constexpr std::size_t Size = 1 << 14;
    std::array<std::uint64_t, Size> m_keys{};
    std::array<std::uint32_t, Size> m_epochs{};
    std::uint32_t m_epoch = 1;
};

std::uint64_t Hash(const Position& p, std::uint32_t extra)
{
    std::uint64_t hash = 1469598103934665603ull ^ extra;
    auto mix = [&](std::int8_t value) {
        hash ^= static_cast<std::uint8_t>(value);
        hash *= 1099511628211ull;
    };
    for (const std::int8_t value : p.points)
        mix(value);
    for (const std::int8_t value : p.bar)
        mix(value);
    for (const std::int8_t value : p.off)
        mix(value);
    return hash;
}

Step MakeStep(int from, int to, int die, bool hit)
{
    return {static_cast<std::int8_t>(from), static_cast<std::int8_t>(to), static_cast<std::int8_t>(die), hit};
}

// The single steps the mover can make with one die.
int StepsFor(const Position& p, Variant variant, int die, std::array<Step, 16>& out)
{
    int count = 0;
    if (variant == Variant::Short && p.bar[0] > 0) {
        // Checkers on the bar enter first, into the opponent's home.
        const int to = FromBar - die;
        if (p.points[static_cast<std::size_t>(to)] >= -1)
            out[static_cast<std::size_t>(count++)] = MakeStep(FromBar, to, die, p.points[static_cast<std::size_t>(to)] == -1);
        return count;
    }
    int highest = -1;
    for (int i = Points - 1; i >= 0 && highest < 0; --i)
        if (p.points[static_cast<std::size_t>(i)] > 0)
            highest = i;
    const bool home = highest < 6;
    for (int from = highest; from >= 0; --from) {
        if (p.points[static_cast<std::size_t>(from)] <= 0)
            continue;
        const int to = from - die;
        if (to >= 0) {
            const int there = p.points[static_cast<std::size_t>(to)];
            if (variant == Variant::Long ? there >= 0 : there >= -1)
                out[static_cast<std::size_t>(count++)] = MakeStep(from, to, die, variant == Variant::Short && there == -1);
        }
        else if (home && (to == BearOff || from == highest)) {
            // Off exactly, or from the farthest checker with a die larger than it needs.
            out[static_cast<std::size_t>(count++)] = MakeStep(from, BearOff, die, false);
        }
    }
    return count;
}

struct Search {
    Variant variant;
    bool distinct; // each position once, for the computer; every order, for a person
    int headLimit;
    std::vector<Move>& nodes;
    SeenSet& seen;
};

std::uint32_t Key(const std::array<int, 4>& dice, int left, int headMoves)
{
    int low = 7, high = 0;
    for (int k = 0; k < left; ++k) {
        low = std::min(low, dice[static_cast<std::size_t>(k)]);
        high = std::max(high, dice[static_cast<std::size_t>(k)]);
    }
    return static_cast<std::uint32_t>(left | low << 3 | high << 6 | headMoves << 9);
}

// Every way of playing the dice still left, each prefix kept as a candidate.
void Explore(Search& s, const Position& p, std::array<int, 4>& dice, int left, Move& current, int headMoves)
{
    current.after = p;
    s.nodes.push_back(current);
    for (int k = 0; k < left; ++k) {
        const int die = dice[static_cast<std::size_t>(k)];
        bool tried = false;
        for (int j = 0; j < k; ++j)
            tried |= dice[static_cast<std::size_t>(j)] == die;
        if (tried)
            continue;
        std::array<Step, 16> steps;
        const int count = StepsFor(p, s.variant, die, steps);
        if (count == 0)
            continue;
        // The die taken out of the ones left: the others stay in front.
        std::swap(dice[static_cast<std::size_t>(k)], dice[static_cast<std::size_t>(left - 1)]);
        for (int i = 0; i < count; ++i) {
            const Step& step = steps[static_cast<std::size_t>(i)];
            const int fromHead = s.variant == Variant::Long && step.from == Head ? 1 : 0;
            if (headMoves + fromHead > s.headLimit)
                continue;
            const Position next = Apply(p, step);
            if (s.distinct && !s.seen.Insert(Hash(next, Key(dice, left - 1, headMoves + fromHead))))
                continue;
            current.steps[static_cast<std::size_t>(current.count++)] = step;
            Explore(s, next, dice, left - 1, current, headMoves + fromHead);
            --current.count;
        }
        std::swap(dice[static_cast<std::size_t>(k)], dice[static_cast<std::size_t>(left - 1)]);
    }
}

std::vector<Move> Collect(const Position& position, Variant variant, Roll roll, bool firstMove, bool distinct)
{
    thread_local SeenSet seen;
    seen.Clear();
    std::vector<Move> nodes;
    nodes.reserve(256);
    const int headLimit = variant == Variant::Short ? 4 : firstMove && roll.Double() && (roll.first == 6 || roll.first == 4 || roll.first == 3) ? 2 : 1;
    Search search{variant, distinct, headLimit, nodes, seen};
    std::array<int, 4> dice{roll.first, roll.second, roll.first, roll.first};
    Move current;
    Explore(search, position, dice, roll.Double() ? 4 : 2, current, 0);

    // As many dice as can be played, by moves that keep to the block rule.
    auto valid = [&](const Move& move) { return variant == Variant::Short || BlockAllowed(move.after); };
    int longest = 0;
    for (const Move& move : nodes)
        if (move.count > longest && valid(move))
            longest = move.count;
    if (longest == 0)
        return {};
    std::vector<Move> moves;
    for (const Move& move : nodes)
        if (move.count == longest && valid(move))
            moves.push_back(move);
    // When only one die can be played, the higher one if it can.
    if (longest == 1 && !roll.Double()) {
        const int higher = std::max(roll.first, roll.second);
        if (std::ranges::any_of(moves, [&](const Move& move) { return move.steps[0].die == higher; }))
            std::erase_if(moves, [&](const Move& move) { return move.steps[0].die != higher; });
    }
    if (distinct) {
        seen.Clear();
        std::erase_if(moves, [&](const Move& move) { return !seen.Insert(Hash(move.after, 0xFFFFu)); });
    }
    return moves;
}

} // namespace

Position Start(Variant variant)
{
    Position p;
    if (variant == Variant::Long) {
        p.points[Head] = Checkers;
        p.points[11] = -Checkers;
    }
    else {
        p.points[23] = 2;
        p.points[12] = 5;
        p.points[7] = 3;
        p.points[5] = 5;
        p.points[0] = -2;
        p.points[11] = -5;
        p.points[16] = -3;
        p.points[18] = -5;
    }
    return p;
}

Position Flip(const Position& p, Variant variant)
{
    // Short nardy is a mirror; in long nardy the opponent's start lies half way round from the mover's.
    Position q;
    for (int i = 0; i < Points; ++i) {
        const int j = variant == Variant::Short ? Points - 1 - i : (i + 12) % Points;
        q.points[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(-p.points[static_cast<std::size_t>(j)]);
    }
    q.bar = {p.bar[1], p.bar[0]};
    q.off = {p.off[1], p.off[0]};
    return q;
}

Position Apply(const Position& p, const Step& step)
{
    Position q = p;
    if (step.from == FromBar)
        --q.bar[0];
    else
        --q.points[static_cast<std::size_t>(step.from)];
    if (step.to == BearOff) {
        ++q.off[0];
    }
    else {
        std::int8_t& there = q.points[static_cast<std::size_t>(step.to)];
        if (there < 0) {
            // Only a lone checker can be there to hit.
            there = 0;
            ++q.bar[1];
        }
        ++there;
    }
    return q;
}

int Pips(const Position& position, int side, Variant variant)
{
    const Position p = side == 0 ? position : Flip(position, variant);
    int pips = p.bar[0] * 25;
    for (int i = 0; i < Points; ++i)
        if (p.points[static_cast<std::size_t>(i)] > 0)
            pips += p.points[static_cast<std::size_t>(i)] * (i + 1);
    return pips;
}

bool BlockAllowed(const Position& p)
{
    // The points in the order the opponent passes them: their last point is the mover's 12, their head the mover's 11.
    auto at = [&](int o) { return p.points[static_cast<std::size_t>((o + 12) % Points)]; };
    int firstOpponent = Points;
    for (int o = 0; o < Points && firstOpponent == Points; ++o)
        if (at(o) < 0)
            firstOpponent = o;
    int run = 0;
    for (int o = 0; o < Points; ++o) {
        run = at(o) > 0 ? run + 1 : 0;
        // Six in a row from o - run + 1: allowed only with an opponent's checker already past its start.
        if (run >= 6 && firstOpponent >= o - run + 1)
            return false;
    }
    return true;
}

std::vector<Move> LegalMoves(const Position& position, Variant variant, Roll roll, bool firstMove)
{
    return Collect(position, variant, roll, firstMove, true);
}

std::vector<Move> LegalSequences(const Position& position, Variant variant, Roll roll, bool firstMove)
{
    return Collect(position, variant, roll, firstMove, false);
}

int WinPoints(const Position& p, Variant variant)
{
    if (p.off[1] > 0)
        return 1;
    if (variant == Variant::Long)
        return 2;
    // A backgammon: the loser still has a checker on the bar or in the winner's home.
    bool deep = p.bar[1] > 0;
    for (int i = 0; i < 6; ++i)
        deep |= p.points[static_cast<std::size_t>(i)] < 0;
    return deep ? 3 : 2;
}

} // namespace nardy
