#pragma once

#include "Nardy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace nardy {

// What a position is worth to the player to move: the chances of winning, of winning a gammon (марс) and of
// losing one.
struct Value {
    float win = 0.5f, winGammon = 0, loseGammon = 0;

    // In points per game.
    float Equity() const { return 2 * win - 1 + winGammon - loseGammon; }
    // The same seen by the other player.
    Value Flipped() const { return {1 - win, loseGammon, winGammon}; }
};

// A small neural network that judges positions, taught by playing against itself (temporal-difference learning, as
// TD-Gammon was): the checkers of both sides on each point, on the bar and borne off, and the pips left, through one
// hidden layer to the three chances of Value.
class Network {
public:
    static constexpr int Inputs = 200, Hidden = 64, Outputs = 3;

    Network();
    void Randomize(std::uint64_t seed);
    bool Load(std::span<const std::byte> data);
    std::vector<std::byte> Save() const;

    Value Evaluate(const Position& position, Variant variant) const;
    // One step of learning toward `target`, at the given rate.
    void Learn(const Position& position, Variant variant, const Value& target, float rate);

private:
    struct Input {
        int index;
        float value;
    };
    using Encoding = std::array<Input, 112>;

    static int Encode(const Position& position, Variant variant, Encoding& inputs);
    void Forward(const Encoding& inputs, int count, std::array<float, Hidden>& hidden, std::array<float, Outputs>& output) const;

    std::vector<float> m_hiddenWeights; // an input's weights to every hidden unit together
    std::vector<float> m_hiddenBias;
    std::vector<float> m_outputWeights; // a hidden unit's weights to every output together
    std::vector<float> m_outputBias;
};

} // namespace nardy
