#include "BoardView.h"

#include "BoardModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

using namespace math;

namespace {

// Quaternions as (x, y, z, w).
Vec4 AxisAngle(Vec3 axis, float angle)
{
    const float s = std::sin(angle / 2);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle / 2)};
}

Vec4 Multiply(Vec4 a, Vec4 b)
{
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Mat4 Rotation(Vec4 q)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat4 m = Mat4::Identity();
    m(0, 0) = 1 - 2 * (y * y + z * z);
    m(0, 1) = 2 * (x * y - w * z);
    m(0, 2) = 2 * (x * z + w * y);
    m(1, 0) = 2 * (x * y + w * z);
    m(1, 1) = 1 - 2 * (x * x + z * z);
    m(1, 2) = 2 * (y * z - w * x);
    m(2, 0) = 2 * (x * z - w * y);
    m(2, 1) = 2 * (y * z + w * x);
    m(2, 2) = 1 - 2 * (x * x + y * y);
    return m;
}

// The turn that brings the face with `value` to the top. The die's faces: 1 on +y, 2 on +z, 3 on +x, and the
// opposite faces adding up to seven.
Vec4 FaceUp(int value)
{
    const float h = std::sqrt(0.5f);
    switch (value) {
    case 2:
        return {-h, 0, 0, h};
    case 3:
        return {0, 0, h, h};
    case 4:
        return {0, 0, -h, h};
    case 5:
        return {h, 0, 0, h};
    case 6:
        return {1, 0, 0, 0};
    default:
        return {0, 0, 0, 1};
    }
}

// The stages of a die's flight where it strikes the board, and how loud.
constexpr std::array<float, 3> Contacts{0.182f, 0.545f, 0.909f};
constexpr std::array<float, 3> ContactVolumes{1.0f, 0.55f, 0.3f};

} // namespace

Vec3 BoardView::Spot(int side, int place, int index) const
{
    if (place == BarPlace)
        return board::BarSpot(side, index);
    if (place == OffPlace)
        return board::OffSpot(side, index);
    return board::CheckerSpot(place, index);
}

void BoardView::Reset(const nardy::Position& board)
{
    m_stacks = {};
    int next = 0;
    auto add = [&](int side, int place, int count) {
        for (int k = 0; k < count && next < 30; ++k) {
            Checker& c = m_checkers[static_cast<std::size_t>(next)];
            auto& stack = m_stacks[static_cast<std::size_t>(side)][static_cast<std::size_t>(place)];
            c = Checker{};
            c.side = side;
            c.place = place;
            c.from = c.to = Spot(side, place, static_cast<int>(stack.size()));
            c.standFrom = place == OffPlace ? 1.0f : 0.0f;
            stack.push_back(next++);
        }
    };
    for (int w = 0; w < nardy::Points; ++w) {
        const int n = board.points[static_cast<std::size_t>(w)];
        add(n > 0 ? 0 : 1, w, std::abs(n));
    }
    for (int side = 0; side < 2; ++side) {
        add(side, BarPlace, board.bar[static_cast<std::size_t>(side)]);
        add(side, OffPlace, board.off[static_cast<std::size_t>(side)]);
    }
    m_liftedSide = m_liftedPlace = -1;
    m_carried.reset();
}

void BoardView::Move(int side, int from, int to, float delay)
{
    auto& source = m_stacks[static_cast<std::size_t>(side)][static_cast<std::size_t>(from)];
    if (source.empty())
        return;
    const int id = source.back();
    source.pop_back();
    Checker& c = m_checkers[static_cast<std::size_t>(id)];
    const Vec3 start = m_carried && side == m_liftedSide && from == m_liftedPlace ? *m_carried : TopOf(side, from);
    // Where it rests, and how long it flies.
    auto& target = m_stacks[static_cast<std::size_t>(side)][static_cast<std::size_t>(to)];
    c.from = start;
    c.standFrom = from == OffPlace ? 1.0f : 0.0f;
    c.to = Spot(side, to, static_cast<int>(target.size()));
    c.place = to;
    c.time = 0;
    c.delay = delay;
    c.duration = 0.3f + 0.35f * std::min(1.0f, Length(c.to - c.from) / 0.5f);
    c.lift = 0;
    c.hitOnArrival = false;
    target.push_back(id);
    if (side == m_liftedSide && from == m_liftedPlace)
        m_liftedSide = m_liftedPlace = -1;
    m_carried.reset();

    // A lone checker of the other side there is knocked to the bar as this one arrives.
    if (to < nardy::Points) {
        auto& there = m_stacks[static_cast<std::size_t>(1 - side)][static_cast<std::size_t>(to)];
        if (!there.empty()) {
            const int hit = there.back();
            there.pop_back();
            Checker& h = m_checkers[static_cast<std::size_t>(hit)];
            auto& bar = m_stacks[static_cast<std::size_t>(1 - side)][BarPlace];
            h.from = TopOf(1 - side, to);
            h.standFrom = 0;
            h.to = Spot(1 - side, BarPlace, static_cast<int>(bar.size()));
            h.place = BarPlace;
            h.time = 0;
            h.delay = delay + c.duration * 0.85f;
            h.duration = 0.45f;
            bar.push_back(hit);
            c.hitOnArrival = true;
        }
    }
}

void BoardView::Lift(int side, int place)
{
    m_liftedSide = side;
    m_liftedPlace = place;
}

void BoardView::Carry(std::optional<Vec3> position)
{
    m_carried = position;
}

void BoardView::ThrowDie(Die& die, int side, int index, int value, Vec3 from, std::mt19937_64& random)
{
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    die = Die{};
    die.visible = true;
    die.side = side;
    die.value = value;
    die.from = from;
    die.to = board::DieSpot(side, index) + Vec3{unit(random) * 0.008f, 0, unit(random) * 0.006f};
    die.rest = Multiply(AxisAngle({0, 1, 0}, unit(random) * 0.5f), FaceUp(value));
    die.spinAxis = Normalize(Vec3{unit(random), unit(random) * 0.4f, unit(random)} + Vec3{0.3f, 0, 0.3f});
    die.spin = 2 * Pi * (2.2f + unit(random) * 0.6f);
    die.duration = 0.95f + 0.12f * unit(random);
    die.time = 0;
}

void BoardView::Throw(int side, int first, int second, std::mt19937_64& random)
{
    // From the thrower's hand, at their side of the board, into their right half.
    const float z = side == 0 ? 0.32f : -0.32f, x = side == 0 ? 0.16f : -0.16f;
    ThrowDie(m_dice[0], side, 0, first, {x - 0.02f, 0.16f, z}, random);
    ThrowDie(m_dice[1], side, 1, second, {x + 0.03f, 0.18f, z + (side == 0 ? 0.02f : -0.02f)}, random);
    m_dice[1].duration += 0.06f;
}

void BoardView::ThrowOpening(int white, int black, std::mt19937_64& random)
{
    ThrowDie(m_dice[0], 0, 0, white, {0.16f, 0.16f, 0.32f}, random);
    ThrowDie(m_dice[1], 1, 0, black, {-0.16f, 0.16f, -0.32f}, random);
}

void BoardView::ShowDice(int side, int first, int second)
{
    std::mt19937_64 random(static_cast<std::uint64_t>(first * 7 + second));
    Throw(side, first, second, random);
    for (Die& die : m_dice)
        die.time = die.duration;
}

void BoardView::UseDice(int used, bool doubles, int firstUsed)
{
    // Two different dice darken one by one; a double's pair after two moves each.
    if (doubles) {
        m_dice[0].dimTarget = used >= 2 ? 1.0f : 0.0f;
        m_dice[1].dimTarget = used >= 4 ? 1.0f : 0.0f;
    }
    else {
        m_dice[0].dimTarget = used >= 2 || (used == 1 && firstUsed == 0) ? 1.0f : 0.0f;
        m_dice[1].dimTarget = used >= 2 || (used == 1 && firstUsed == 1) ? 1.0f : 0.0f;
    }
}

void BoardView::Update(float seconds)
{
    for (std::size_t id = 0; id < m_checkers.size(); ++id) {
        Checker& c = m_checkers[id];
        if (c.time < 1) {
            if (c.delay > 0) {
                c.delay -= seconds;
            }
            else {
                c.time = std::min(1.0f, c.time + seconds / c.duration);
                if (c.time >= 1) {
                    if (c.hitOnArrival)
                        ++m_sounds.hits;
                    else if (c.place == OffPlace)
                        ++m_sounds.trays;
                    else
                        ++m_sounds.landings;
                }
            }
        }
        const auto& stack = m_stacks[static_cast<std::size_t>(c.side)][static_cast<std::size_t>(c.place)];
        const bool lifted = c.side == m_liftedSide && c.place == m_liftedPlace && !stack.empty() && stack.back() == static_cast<int>(id);
        c.lift += ((lifted ? 1.0f : 0.0f) - c.lift) * (1 - std::exp(-seconds * 18));
    }
    for (Die& die : m_dice) {
        if (!die.visible)
            continue;
        if (die.time < die.duration) {
            const float before = die.time / die.duration;
            die.time = std::min(die.duration, die.time + seconds);
            const float after = die.time / die.duration;
            for (std::size_t k = 0; k < Contacts.size(); ++k)
                if (before < Contacts[k] && after >= Contacts[k])
                    m_sounds.dice.push_back(ContactVolumes[k]);
        }
        die.dim += (die.dimTarget - die.dim) * (1 - std::exp(-seconds * 10));
    }
}

bool BoardView::Animating() const
{
    for (std::size_t id = 0; id < m_checkers.size(); ++id) {
        const Checker& c = m_checkers[id];
        if (c.time < 1)
            return true;
        const auto& stack = m_stacks[static_cast<std::size_t>(c.side)][static_cast<std::size_t>(c.place)];
        const bool lifted = c.side == m_liftedSide && c.place == m_liftedPlace && !stack.empty() && stack.back() == static_cast<int>(id);
        if (std::abs((lifted ? 1.0f : 0.0f) - c.lift) > 0.002f)
            return true;
    }
    return std::ranges::any_of(m_dice, [](const Die& die) { return die.visible && (die.time < die.duration || std::abs(die.dim - die.dimTarget) > 0.002f); });
}

bool BoardView::DiceRolling() const
{
    return std::ranges::any_of(m_dice, [](const Die& die) { return die.visible && die.time < die.duration; });
}

BoardView::Sounds BoardView::TakeSounds()
{
    return std::exchange(m_sounds, Sounds{});
}

Mat4 BoardView::CheckerModel(const Checker& c) const
{
    // In flight: along an arc that rises with the distance; standing on edge in the tray.
    const float s = SmoothStep(0, 1, c.time);
    const float distance = Length(c.to - c.from);
    const float arc = std::min(0.12f, 0.025f + 0.12f * distance);
    Vec3 p = Lerp(c.from, c.to, s) + Vec3{0, arc * 4 * s * (1 - s), 0};
    p.y += c.lift * 0.014f;
    if (m_carried && c.side == m_liftedSide && c.place == m_liftedPlace && c.time >= 1) {
        // Only the top checker of the place is carried.
        const auto& stack = m_stacks[static_cast<std::size_t>(c.side)][static_cast<std::size_t>(c.place)];
        if (!stack.empty() && &m_checkers[static_cast<std::size_t>(stack.back())] == &c)
            p = *m_carried;
    }
    const float stand = Lerp(c.standFrom, c.place == OffPlace ? 1.0f : 0.0f, s);
    return Mat4::Translation(p) * Mat4::RotationX(stand * Pi / 2) * Mat4::Scale({board::CheckerRadius, board::CheckerRadius, board::CheckerRadius});
}

Vec3 BoardView::TopOf(int side, int place) const
{
    const auto& stack = m_stacks[static_cast<std::size_t>(side)][static_cast<std::size_t>(place)];
    if (stack.empty())
        return Spot(side, place, 0);
    const Checker& c = m_checkers[static_cast<std::size_t>(stack.back())];
    const Mat4 model = CheckerModel(c);
    return {model(0, 3), model(1, 3), model(2, 3)};
}

void BoardView::AddPieces(std::vector<Piece>& pieces, const std::array<float, Places>& glow, int glowSide) const
{
    for (int side = 0; side < 2; ++side) {
        for (int place = 0; place < Places; ++place) {
            const auto& stack = m_stacks[static_cast<std::size_t>(side)][static_cast<std::size_t>(place)];
            for (std::size_t k = 0; k < stack.size(); ++k) {
                const Checker& c = m_checkers[static_cast<std::size_t>(stack[k])];
                Piece piece;
                piece.model = CheckerModel(c);
                piece.side = side;
                const bool top = k + 1 == stack.size();
                piece.highlight = std::max(top && c.time >= 1 && side == glowSide ? glow[static_cast<std::size_t>(place)] : 0.0f, c.lift);
                pieces.push_back(piece);
            }
        }
    }
    for (const Die& die : m_dice) {
        if (!die.visible)
            continue;
        const float s = die.time / die.duration;
        // Fast spin at first, slowing as it settles on the face rolled.
        const float spinLeft = 1 - (1 - std::pow(1 - s, 3.0f));
        const Vec4 q = Multiply(die.rest, AxisAngle(die.spinAxis, die.spin * spinLeft));
        const Mat4 rotation = Rotation(q);
        const float extent = board::DieSize * (std::abs(rotation(1, 0)) + std::abs(rotation(1, 1)) + std::abs(rotation(1, 2)));
        const float travel = 1 - std::pow(1 - s, 2.2f);
        const float bounce = die.from.y * (1 - s) * (1 - s) * std::abs(std::cos(s * Pi * 2.75f));
        Vec3 p = Lerp(die.from, die.to, travel);
        p.y = std::max(extent * 0.92f, board::DieSize) + bounce;
        Piece piece;
        piece.model = Mat4::Translation(p) * rotation * Mat4::Scale({board::DieSize, board::DieSize, board::DieSize});
        piece.kind = 1;
        piece.side = die.side;
        piece.value = die.value;
        piece.dim = die.dim;
        pieces.push_back(piece);
    }
}
