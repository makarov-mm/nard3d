#include "Network.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace nardy {

namespace {

constexpr char Magic[4] = {'N', 'R', 'D', '1'};

float Sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

} // namespace

Network::Network()
    : m_hiddenWeights(static_cast<std::size_t>(Inputs * Hidden)), m_hiddenBias(Hidden), m_outputWeights(static_cast<std::size_t>(Hidden * Outputs)),
      m_outputBias(Outputs)
{
}

void Network::Randomize(std::uint64_t seed)
{
    std::mt19937_64 random(seed);
    std::uniform_real_distribution<float> small(-0.1f, 0.1f);
    for (float& w : m_hiddenWeights)
        w = small(random);
    for (float& w : m_outputWeights)
        w = small(random);
    std::ranges::fill(m_hiddenBias, 0.0f);
    std::ranges::fill(m_outputBias, 0.0f);
}

bool Network::Load(std::span<const std::byte> data)
{
    const std::size_t floats = m_hiddenWeights.size() + m_hiddenBias.size() + m_outputWeights.size() + m_outputBias.size();
    if (data.size() != 16 + floats * sizeof(float) || std::memcmp(data.data(), Magic, 4) != 0)
        return false;
    std::int32_t shape[3];
    std::memcpy(shape, data.data() + 4, sizeof(shape));
    if (shape[0] != Inputs || shape[1] != Hidden || shape[2] != Outputs)
        return false;
    const std::byte* at = data.data() + 16;
    for (std::vector<float>* part : {&m_hiddenWeights, &m_hiddenBias, &m_outputWeights, &m_outputBias}) {
        std::memcpy(part->data(), at, part->size() * sizeof(float));
        at += part->size() * sizeof(float);
    }
    return true;
}

std::vector<std::byte> Network::Save() const
{
    std::vector<std::byte> data(16);
    std::memcpy(data.data(), Magic, 4);
    const std::int32_t shape[3] = {Inputs, Hidden, Outputs};
    std::memcpy(data.data() + 4, shape, sizeof(shape));
    for (const std::vector<float>* part : {&m_hiddenWeights, &m_hiddenBias, &m_outputWeights, &m_outputBias}) {
        const std::size_t at = data.size();
        data.resize(at + part->size() * sizeof(float));
        std::memcpy(data.data() + at, part->data(), part->size() * sizeof(float));
    }
    return data;
}

// The inputs that are not zero: for every point four for each side (one checker, two, three, and the rest), then the
// bar, the checkers borne off, the pips left and whether each side may bear off.
int Network::Encode(const Position& p, Variant variant, Encoding& inputs)
{
    int count = 0;
    auto add = [&](int index, float value) {
        if (value != 0)
            inputs[static_cast<std::size_t>(count++)] = {index, value};
    };
    for (int i = 0; i < Points; ++i) {
        const int n = p.points[static_cast<std::size_t>(i)];
        const int base = i * 8 + (n > 0 ? 0 : 4);
        const int m = std::abs(n);
        add(base, m >= 1 ? 1.0f : 0.0f);
        add(base + 1, m >= 2 ? 1.0f : 0.0f);
        add(base + 2, m >= 3 ? 1.0f : 0.0f);
        add(base + 3, m > 3 ? static_cast<float>(m - 3) * 0.5f : 0.0f);
    }
    const float startPips = variant == Variant::Long ? 360.0f : 167.0f;
    add(192, p.bar[0] * 0.5f);
    add(193, p.bar[1] * 0.5f);
    add(194, p.off[0] / 15.0f);
    add(195, p.off[1] / 15.0f);
    add(196, static_cast<float>(Pips(p, 0, variant)) / startPips);
    add(197, static_cast<float>(Pips(p, 1, variant)) / startPips);
    // Whether each side has every checker home.
    bool mine = p.bar[0] == 0, theirs = p.bar[1] == 0;
    for (int i = 6; i < Points; ++i)
        mine &= p.points[static_cast<std::size_t>(i)] <= 0;
    const Position flipped = Flip(p, variant);
    for (int i = 6; i < Points; ++i)
        theirs &= flipped.points[static_cast<std::size_t>(i)] <= 0;
    add(198, mine ? 1.0f : 0.0f);
    add(199, theirs ? 1.0f : 0.0f);
    return count;
}

void Network::Forward(const Encoding& inputs, int count, std::array<float, Hidden>& hidden, std::array<float, Outputs>& output) const
{
    std::copy(m_hiddenBias.begin(), m_hiddenBias.end(), hidden.begin());
    for (int k = 0; k < count; ++k) {
        const Input& input = inputs[static_cast<std::size_t>(k)];
        const float* weights = &m_hiddenWeights[static_cast<std::size_t>(input.index * Hidden)];
        for (int j = 0; j < Hidden; ++j)
            hidden[static_cast<std::size_t>(j)] += weights[j] * input.value;
    }
    for (float& h : hidden)
        h = Sigmoid(h);
    for (int o = 0; o < Outputs; ++o) {
        float sum = m_outputBias[static_cast<std::size_t>(o)];
        for (int j = 0; j < Hidden; ++j)
            sum += hidden[static_cast<std::size_t>(j)] * m_outputWeights[static_cast<std::size_t>(j * Outputs + o)];
        output[static_cast<std::size_t>(o)] = Sigmoid(sum);
    }
}

Value Network::Evaluate(const Position& position, Variant variant) const
{
    Encoding inputs;
    const int count = Encode(position, variant, inputs);
    std::array<float, Hidden> hidden;
    std::array<float, Outputs> output;
    Forward(inputs, count, hidden, output);
    // A gammon is a win too: its chance cannot be above the win's.
    return {output[0], std::min(output[1], output[0]), std::min(output[2], 1 - output[0])};
}

void Network::Learn(const Position& position, Variant variant, const Value& target, float rate)
{
    Encoding inputs;
    const int count = Encode(position, variant, inputs);
    std::array<float, Hidden> hidden;
    std::array<float, Outputs> output;
    Forward(inputs, count, hidden, output);
    // Cross-entropy on each sigmoid output: its error is simply target minus output.
    const std::array<float, Outputs> error{target.win - output[0], target.winGammon - output[1], target.loseGammon - output[2]};
    std::array<float, Hidden> back{};
    for (int j = 0; j < Hidden; ++j) {
        float sum = 0;
        for (int o = 0; o < Outputs; ++o) {
            float& w = m_outputWeights[static_cast<std::size_t>(j * Outputs + o)];
            sum += w * error[static_cast<std::size_t>(o)];
            w += rate * error[static_cast<std::size_t>(o)] * hidden[static_cast<std::size_t>(j)];
        }
        back[static_cast<std::size_t>(j)] = sum * hidden[static_cast<std::size_t>(j)] * (1 - hidden[static_cast<std::size_t>(j)]);
    }
    for (int o = 0; o < Outputs; ++o)
        m_outputBias[static_cast<std::size_t>(o)] += rate * error[static_cast<std::size_t>(o)];
    for (int k = 0; k < count; ++k) {
        const Input& input = inputs[static_cast<std::size_t>(k)];
        float* weights = &m_hiddenWeights[static_cast<std::size_t>(input.index * Hidden)];
        for (int j = 0; j < Hidden; ++j)
            weights[j] += rate * back[static_cast<std::size_t>(j)] * input.value;
    }
    for (int j = 0; j < Hidden; ++j)
        m_hiddenBias[static_cast<std::size_t>(j)] += rate * back[static_cast<std::size_t>(j)];
}

} // namespace nardy
