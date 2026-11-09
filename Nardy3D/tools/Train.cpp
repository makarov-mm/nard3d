// Teaches a position network by playing against itself (temporal-difference learning, as TD-Gammon was taught), and
// measures it against a plain greedy player or another network. It is not part of the game: the networks it makes are
// built into the game from assets/long.net and assets/short.net.
//
//   Train long|short games out.net [in.net|-] [rival.net|-] [rate scale]
//       teaches for `games` games, from a random network or from in.net, measuring it every 25000 games
//   Train long|short compare a.net b.net|greedy games
//       plays a.net against b.net, or against the greedy player
//
// The game's networks: short nardy 800000 games, then 1000000 more at rate scale 0.3; long nardy 300000 games, then
// 400000 more at rate scale 0.3 and 400000 at 0.15.
#include "../src/Network.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <span>
#include <vector>

using namespace nardy;

namespace {

Roll Throw(std::mt19937_64& random)
{
    std::uniform_int_distribution<int> die(1, 6);
    return {die(random), die(random)};
}

Value Terminal(const Position& after, Variant variant)
{
    return {1, WinPoints(after, variant) >= 2 ? 1.0f : 0.0f, 0};
}

// The value to the mover of a move's result.
Value MoveValue(const Network& net, const Move& move, Variant variant)
{
    if (move.after.off[0] == Checkers)
        return Terminal(move.after, variant);
    return net.Evaluate(Flip(move.after, variant), variant).Flipped();
}

// A plain player: as far forward as it can, hitting and covering in short nardy.
int Greedy(const std::vector<Move>& moves, Variant variant, std::mt19937_64& random)
{
    int best = 0;
    double bestScore = -1e9;
    std::uniform_real_distribution<double> tie(0, 0.01);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const Position& p = moves[i].after;
        double score = -Pips(p, 0, variant) + 25.0 * p.off[0];
        if (variant == Variant::Short) {
            score += 15.0 * p.bar[1];
            for (int k = 0; k < Points; ++k)
                if (p.points[static_cast<std::size_t>(k)] == 1)
                    score -= 4.0 + 0.4 * k;
        }
        score += tie(random);
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// One game; side 0 plays by the network, side 1 by the network too (training), by another network, or greedily.
// Returns the points side 0 won, negative for a loss.
int Play(Network& net, Variant variant, std::mt19937_64& random, bool learn, float rate, bool againstGreedy, const Network* rival = nullptr)
{
    Position p = Start(variant);
    std::array<bool, 2> first{true, true};
    int side = static_cast<int>(random() % 2);
    Roll roll = Throw(random);
    if (variant == Variant::Short)
        while (roll.Double())
            roll = Throw(random);
    for (int turn = 0; turn < 3000; ++turn) {
        if (turn > 0)
            roll = Throw(random);
        const std::vector<Move> moves = LegalMoves(p, variant, roll, first[static_cast<std::size_t>(side)]);
        first[static_cast<std::size_t>(side)] = false;
        Position next;
        Value target;
        bool over = false;
        if (moves.empty()) {
            next = Flip(p, variant);
            target = net.Evaluate(next, variant).Flipped();
        }
        else {
            int chosen = 0;
            if (againstGreedy && side == 1 && !rival) {
                chosen = Greedy(moves, variant, random);
                target = MoveValue(net, moves[static_cast<std::size_t>(chosen)], variant);
            }
            else {
                const Network& judge = againstGreedy && side == 1 ? *rival : net;
                float best = -10;
                for (std::size_t i = 0; i < moves.size(); ++i) {
                    const Value value = MoveValue(judge, moves[i], variant);
                    if (value.Equity() > best) {
                        best = value.Equity();
                        chosen = static_cast<int>(i);
                        target = value;
                    }
                }
            }
            const Move& move = moves[static_cast<std::size_t>(chosen)];
            over = move.after.off[0] == Checkers;
            next = Flip(move.after, variant);
            if (over) {
                const int points = WinPoints(move.after, variant);
                if (learn)
                    net.Learn(p, variant, target, rate);
                return side == 0 ? points : -points;
            }
        }
        if (learn)
            net.Learn(p, variant, target, rate);
        p = next;
        side = 1 - side;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::printf("Train long|short games out.net [in.net|-] [rival.net|-] [rate scale]\nTrain long|short compare a.net b.net|greedy games\n");
        return 1;
    }
    const Variant variant = std::strcmp(argv[1], "long") == 0 ? Variant::Long : Variant::Short;
    auto load = [](const char* path, Network& into) {
        std::ifstream in(path, std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!into.Load(std::span(reinterpret_cast<const std::byte*>(raw.data()), raw.size())))
            std::printf("could not load %s\n", path);
    };
    if (std::strcmp(argv[2], "compare") == 0 && argc > 5) {
        Network a, b;
        load(argv[3], a);
        const bool greedy = std::strcmp(argv[4], "greedy") == 0;
        if (!greedy)
            load(argv[4], b);
        const long matches = std::atol(argv[5]);
        long points = 0, wins = 0;
        std::mt19937_64 test(2024);
        for (long k = 0; k < matches; ++k) {
            const int result = Play(a, variant, test, false, 0, true, greedy ? nullptr : &b);
            points += result;
            wins += result > 0;
        }
        std::printf("%s against %s: %.1f%% won, %+.3f points a game\n", argv[3], argv[4], 100.0 * static_cast<double>(wins) / static_cast<double>(matches),
                    static_cast<double>(points) / static_cast<double>(matches));
        return 0;
    }
    const long games = std::atol(argv[2]);
    const float scale = argc > 6 ? static_cast<float>(std::atof(argv[6])) : 1.0f;
    Network net;
    net.Randomize(12345);
    if (argc > 4 && std::strcmp(argv[4], "-") != 0)
        load(argv[4], net);
    // A network to measure against, instead of the greedy player.
    Network rival;
    const bool hasRival = argc > 5 && std::strcmp(argv[5], "-") != 0;
    if (hasRival)
        load(argv[5], rival);
    std::mt19937_64 random(987654321);
    auto save = [&] {
        const std::vector<std::byte> data = net.Save();
        std::ofstream out(argv[3], std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    };
    const auto start = std::chrono::steady_clock::now();
    for (long game = 1; game <= games; ++game) {
        const double progress = static_cast<double>(game) / static_cast<double>(games);
        const float rate = scale * (progress < 0.3 ? 0.04f : progress < 0.7 ? 0.02f : 0.008f);
        Play(net, variant, random, true, rate, false);
        if (game % 25000 == 0 || game == games) {
            int points = 0, wins = 0;
            std::mt19937_64 test(game);
            const int matches = 400;
            for (int k = 0; k < matches; ++k) {
                const int result = Play(net, variant, test, false, 0, true, hasRival ? &rival : nullptr);
                points += result;
                wins += result > 0;
            }
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::printf("%ld games, %.0f s: against the %s %.1f%% won, %+.3f points a game\n", game, seconds, hasRival ? "rival" : "greedy player", 100.0 * wins / matches,
                        static_cast<double>(points) / matches);
            std::fflush(stdout);
            save();
        }
    }
    save();
}
