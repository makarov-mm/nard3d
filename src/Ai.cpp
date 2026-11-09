#include "Ai.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <random>
#include <vector>

namespace nardy {

namespace {

// The worth to the mover of where a move leaves the board, in points.
float Judge(const Network& net, const Move& move, Variant variant)
{
    if (move.after.off[0] == Checkers)
        return static_cast<float>(WinPoints(move.after, variant));
    return net.Evaluate(Flip(move.after, variant), variant).Flipped().Equity();
}

// The worth to the mover of a move, averaged over the opponent's rolls and their best answer to each.
float LookAhead(const Network& net, const Move& move, Variant variant, bool opponentFirst, std::stop_token stop)
{
    if (move.after.off[0] == Checkers)
        return static_cast<float>(WinPoints(move.after, variant));
    const Position theirs = Flip(move.after, variant);
    float total = 0;
    for (int a = 1; a <= 6; ++a) {
        for (int b = a; b <= 6; ++b) {
            if (stop.stop_requested())
                return 0;
            const float weight = a == b ? 1.0f / 36 : 2.0f / 36;
            const std::vector<Move> replies = LegalMoves(theirs, variant, {a, b}, opponentFirst);
            float best;
            if (replies.empty()) {
                // They cannot move: the mover is to play again.
                best = -net.Evaluate(move.after, variant).Equity();
            }
            else {
                best = -1e9f;
                for (const Move& reply : replies)
                    best = std::max(best, Judge(net, reply, variant));
            }
            total -= weight * best;
        }
    }
    return total;
}

} // namespace

Move Ai::Choose(const Network& net, const Position& position, Variant variant, Roll roll, bool firstMove, bool opponentFirst, int level,
                std::uint64_t seed, std::stop_token stop)
{
    const std::vector<Move> moves = LegalMoves(position, variant, roll, firstMove);
    if (moves.empty())
        return {};
    std::mt19937_64 random(seed);
    // How far off each level's judgement is, in points.
    static constexpr float Noise[6] = {0, 0.45f, 0.22f, 0.08f, 0, 0};
    const float noise = Noise[std::clamp(level, 1, 5)];
    std::normal_distribution<float> error(0, noise > 0 ? noise : 1);
    if (level <= 1 && random() % 4 == 0)
        return moves[random() % moves.size()];

    std::vector<std::pair<float, std::size_t>> ranked;
    for (std::size_t i = 0; i < moves.size(); ++i)
        ranked.emplace_back(Judge(net, moves[i], variant) + (noise > 0 ? error(random) : 0), i);
    std::ranges::sort(ranked, std::greater{});
    if (level < 5 || moves.size() == 1)
        return moves[ranked.front().second];

    // The strongest level looks one roll further at the most promising few.
    const std::size_t considered = std::min<std::size_t>(ranked.size(), 8);
    float best = -1e9f;
    std::size_t chosen = ranked.front().second;
    for (std::size_t k = 0; k < considered && !stop.stop_requested(); ++k) {
        const float value = LookAhead(net, moves[ranked[k].second], variant, opponentFirst, stop);
        if (value > best) {
            best = value;
            chosen = ranked[k].second;
        }
    }
    return moves[chosen];
}

} // namespace nardy
