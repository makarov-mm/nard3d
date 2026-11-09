#include "BoardModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace board {

using namespace math;

namespace {

constexpr float TrayInner = FieldRight + RimWidth;
constexpr float TrayOuter = TrayInner + TrayWidth;
constexpr float TrayWall = 0.008f;
constexpr float TrayFloor = 0.006f;
constexpr float Outer = HalfDepth + RimWidth; // the board's far and near edges

// Adds a triangle, wound counterclockwise as seen from the side its normals face.
void AddTriangle(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c)
{
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), {a, b, c});
    const Vec3 facing = Cross(b.position - a.position, c.position - a.position);
    if (Dot(facing, a.normal + b.normal + c.normal) >= 0)
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
    else
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1});
}

void AddQuad(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    AddTriangle(mesh, a, b, c);
    AddTriangle(mesh, a, c, d);
}

// A box; without its top where something lies on it.
void AddBox(Mesh& mesh, Vec3 low, Vec3 high, bool top = true)
{
    const Vec3 c[8] = {{low.x, low.y, low.z}, {high.x, low.y, low.z}, {high.x, high.y, low.z}, {low.x, high.y, low.z},
                       {low.x, low.y, high.z}, {high.x, low.y, high.z}, {high.x, high.y, high.z}, {low.x, high.y, high.z}};
    auto face = [&](int a, int b, int d, int e, Vec3 normal) {
        auto v = [&](int i) { return Vertex{c[i], normal, {c[i].x + c[i].z, c[i].y + c[i].z}}; };
        AddQuad(mesh, v(a), v(b), v(d), v(e));
    };
    face(0, 1, 2, 3, {0, 0, -1});
    face(4, 5, 6, 7, {0, 0, 1});
    face(0, 4, 7, 3, {-1, 0, 0});
    face(1, 5, 6, 2, {1, 0, 0});
    face(0, 1, 5, 4, {0, -1, 0});
    if (top)
        face(3, 2, 6, 7, {0, 1, 0});
}

// The normals along a 2D outline that runs counterclockwise round a solid: across each segment, turned a quarter to
// the right of its direction, and shared where neighbouring segments meet at a slight angle, so that rounded parts
// shade smoothly and sharp ones stay sharp.
struct OutlineVertex {
    Vec2 point;
    Vec2 normal;
    float along;
};

std::vector<std::array<OutlineVertex, 2>> Segments(const std::vector<Vec2>& outline)
{
    std::vector<Vec2> normals;
    for (std::size_t i = 0; i + 1 < outline.size(); ++i) {
        const Vec2 d = outline[i + 1] - outline[i];
        const float length = std::max(Length(d), 1e-9f);
        normals.push_back({d.y / length, -d.x / length});
    }
    std::vector<std::array<OutlineVertex, 2>> segments;
    float along = 0;
    for (std::size_t i = 0; i < normals.size(); ++i) {
        auto shared = [&](std::size_t other) {
            const Vec2 sum = normals[i] + normals[other];
            return Dot(normals[i], normals[other]) > 0.77f ? sum * (1 / Length(sum)) : normals[i];
        };
        const Vec2 n0 = i > 0 ? shared(i - 1) : normals[i];
        const Vec2 n1 = i + 1 < normals.size() ? shared(i + 1) : normals[i];
        const float length = Length(outline[i + 1] - outline[i]);
        segments.push_back({OutlineVertex{outline[i], n0, along}, OutlineVertex{outline[i + 1], n1, along + length}});
        along += length;
    }
    return segments;
}

// A surface of revolution round the y axis from an outline of (radius, height), listed from the axis at the bottom
// round the outside to the axis at the top.
Mesh Lathe(const std::vector<Vec2>& outline, int turns)
{
    Mesh mesh;
    for (const auto& segment : Segments(outline)) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (const OutlineVertex& v : segment) {
            for (int j = 0; j <= turns; ++j) {
                const float t = static_cast<float>(j) / static_cast<float>(turns);
                const float c = std::cos(2 * Pi * t), s = std::sin(2 * Pi * t);
                mesh.vertices.push_back({{v.point.x * c, v.point.y, v.point.x * s}, Normalize(Vec3{v.normal.x * c, v.normal.y, v.normal.x * s}), {t, v.along}});
            }
        }
        const auto row = static_cast<std::uint32_t>(turns + 1);
        for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(turns); ++j) {
            const std::uint32_t a = base + j, b = base + j + 1, c = base + row + j + 1, d = base + row + j;
            // Counterclockwise seen from outside, with the outline running upward on the outside.
            mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
        }
    }
    return mesh;
}

// A rim along the straight edge from a to b of the plane y = 0 (as x, z), `outward` pointing away from the field,
// with the cross-section `outline` of (distance outward, height) running up from the field and over the top, the
// other way round from a lathe's outline. Its ends are cut at 45 degrees where `mitre` says, so that four make a frame.
void Sweep(Mesh& mesh, Vec2 a, Vec2 b, Vec2 outward, const std::vector<Vec2>& outline, bool mitreA, bool mitreB)
{
    const Vec2 span = b - a;
    const Vec2 along = span * (1 / Length(span));
    auto place = [&](Vec2 p, bool atB) {
        const float extend = (atB ? mitreB : mitreA) ? p.x : 0.0f;
        const Vec2 at = (atB ? b + along * extend : a - along * extend) + outward * p.x;
        return Vec3{at.x, p.y, at.y};
    };
    for (const auto& segment : Segments(outline)) {
        auto vertex = [&](const OutlineVertex& v, bool atB) {
            const Vec3 position = place(v.point, atB);
            const Vec3 normal = Normalize(Vec3{-outward.x * v.normal.x, -v.normal.y, -outward.y * v.normal.x});
            return Vertex{position, normal, {Dot(Vec2{position.x, position.z}, along), v.along}};
        };
        AddQuad(mesh, vertex(segment[0], false), vertex(segment[1], false), vertex(segment[1], true), vertex(segment[0], true));
    }
}

// The cross-section of a rim: up from the field, a rounded top, and down the outside to the bottom of the base.
std::vector<Vec2> RimOutline(float width)
{
    std::vector<Vec2> outline{{0, 0}, {0, RimHeight - 0.004f}};
    const float round = 0.004f;
    for (int k = 1; k <= 4; ++k) {
        const float angle = Pi / 2 * static_cast<float>(k) / 4;
        outline.push_back({round - round * std::cos(angle), RimHeight - round + round * std::sin(angle)});
    }
    const float outer = 0.007f;
    outline.push_back({width - outer, RimHeight});
    for (int k = 1; k <= 6; ++k) {
        const float angle = Pi / 2 * static_cast<float>(k) / 6;
        outline.push_back({width - outer + outer * std::sin(angle), RimHeight - outer + outer * std::cos(angle)});
    }
    outline.push_back({width, -Base});
    return outline;
}

} // namespace

Vec3 PointBase(int w)
{
    const bool nearSide = w < 12;
    const int column = nearSide ? w : 23 - w; // from the right
    const float x = column < 6 ? BarHalf + (5.5f - static_cast<float>(column)) * PointWidth : -(BarHalf + (static_cast<float>(column) - 5.5f) * PointWidth);
    return {x, 0, nearSide ? HalfDepth : -HalfDepth};
}

float PointToward(int w)
{
    return w < 12 ? -1.0f : 1.0f;
}

Vec3 CheckerSpot(int w, int index)
{
    const int slot = index % RowLength, layer = index / RowLength;
    Vec3 p = PointBase(w);
    p.z += PointToward(w) * ((static_cast<float>(slot) + 0.5f) * CheckerStep + 0.002f);
    p.y = static_cast<float>(layer) * CheckerHeight;
    return p;
}

Vec3 BarSpot(int side, int index)
{
    const int slot = index % 4, layer = index / 4;
    const float z = 0.045f + static_cast<float>(slot) * CheckerStep;
    return {0, RimHeight + static_cast<float>(layer) * CheckerHeight, side == 0 ? -z : z};
}

Vec3 OffSpot(int side, int index)
{
    const float z = Outer - TrayWall - 0.006f - (static_cast<float>(index) + 0.5f) * (CheckerHeight + 0.0006f);
    return {(TrayInner + TrayOuter - TrayWall) / 2, TrayFloor + CheckerRadius, side == 0 ? z : -z};
}

Vec3 DieSpot(int side, int index)
{
    const float x = BarHalf + 3 * PointWidth + (index == 0 ? -0.032f : 0.034f);
    const float z = index == 0 ? -0.006f : 0.009f;
    return {side == 0 ? x : -x, DieSize, side == 0 ? z : -z};
}

int PointAt(Vec2 xz)
{
    const float ax = std::abs(xz.x);
    if (ax < BarHalf || ax > FieldRight || std::abs(xz.y) > HalfDepth)
        return -1;
    const int across = std::min(5, static_cast<int>((ax - BarHalf) / PointWidth));
    const int column = xz.x > 0 ? 5 - across : 6 + across;
    return xz.y > 0 ? column : 23 - column;
}

BoardMeshes BuildBoard()
{
    BoardMeshes meshes;

    // The two playing surfaces; the points are drawn on them.
    for (const float side : {-1.0f, 1.0f}) {
        auto v = [&](float x, float z) { return Vertex{{x, 0, z}, {0, 1, 0}, {x, z}}; };
        const float x0 = side * BarHalf, x1 = side * FieldRight;
        AddQuad(meshes.field, v(x0, -HalfDepth), v(x1, -HalfDepth), v(x1, HalfDepth), v(x0, HalfDepth));
    }

    // The rim round the board, the middle bar between the halves, and the base under them.
    const std::vector<Vec2> rim = RimOutline(RimWidth);
    const Vec2 corners[4] = {{-FieldRight, HalfDepth}, {FieldRight, HalfDepth}, {FieldRight, -HalfDepth}, {-FieldRight, -HalfDepth}};
    const Vec2 outwards[4] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (int k = 0; k < 4; ++k)
        Sweep(meshes.wood, corners[k], corners[(k + 1) % 4], outwards[k], rim, true, true);
    std::vector<Vec2> bar{{0, 0}, {0, RimHeight - 0.004f}};
    for (int k = 1; k <= 4; ++k) {
        const float angle = Pi / 2 * static_cast<float>(k) / 4;
        bar.push_back({0.004f - 0.004f * std::cos(angle), RimHeight - 0.004f + 0.004f * std::sin(angle)});
    }
    bar.push_back({BarHalf, RimHeight});
    // Each half of the bar from its field's edge to the middle line.
    for (const float side : {-1.0f, 1.0f})
        Sweep(meshes.wood, {side * BarHalf, -HalfDepth}, {side * BarHalf, HalfDepth}, {-side, 0}, bar, false, false);
    AddBox(meshes.wood, {-FieldRight - RimWidth, -Base, -Outer}, {TrayOuter, -0.001f, Outer}, false);

    // The tray beside the right half: an outer wall, end walls, a divider, and a floor of felt in each compartment.
    AddBox(meshes.wood, {TrayOuter - TrayWall, -Base, -Outer}, {TrayOuter, RimHeight, Outer});
    for (const float z : {-Outer, Outer - TrayWall})
        AddBox(meshes.wood, {TrayInner, -Base, z}, {TrayOuter - TrayWall, RimHeight, z + TrayWall});
    AddBox(meshes.wood, {TrayInner, -Base, -TrayWall / 2}, {TrayOuter - TrayWall, RimHeight, TrayWall / 2});
    AddBox(meshes.wood, {TrayInner, -Base, -Outer}, {TrayOuter - TrayWall, TrayFloor - 0.0005f, Outer}, false);
    for (const float side : {-1.0f, 1.0f}) {
        auto v = [&](float x, float z) { return Vertex{{x, TrayFloor, z}, {0, 1, 0}, {x, z}}; };
        const float z0 = side * TrayWall / 2, z1 = side * (Outer - TrayWall);
        AddQuad(meshes.felt, v(TrayInner, z0), v(TrayOuter - TrayWall, z0), v(TrayOuter - TrayWall, z1), v(TrayInner, z1));
    }

    // The table, wide enough to fill the view.
    auto t = [&](float x, float z) { return Vertex{{x, -Base, z}, {0, 1, 0}, {x, z}}; };
    AddQuad(meshes.table, t(-4, -4), t(4, -4), t(4, 4), t(-4, 4));
    return meshes;
}

Mesh BuildChecker()
{
    // Rounded edges, a raised ring round the top and a shallow dish inside it with a carved groove.
    const float h = CheckerHeight / CheckerRadius;
    std::vector<Vec2> outline{{0, 0}, {0.9f, 0}};
    auto arc = [&](Vec2 center, float radius, float from, float to, int steps) {
        for (int k = 1; k <= steps; ++k) {
            const float angle = from + (to - from) * static_cast<float>(k) / static_cast<float>(steps);
            outline.push_back({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
        }
    };
    arc({0.9f, 0.1f}, 0.1f, -Pi / 2, 0, 4);
    outline.push_back({1.0f, h - 0.1f});
    arc({0.9f, h - 0.1f}, 0.1f, 0, Pi / 2, 4);
    outline.push_back({0.8f, h});
    arc({0.8f, h - 0.04f}, 0.04f, Pi / 2, Pi, 3);
    outline.push_back({0.74f, h - 0.06f});
    outline.push_back({0.5f, h - 0.065f});
    outline.push_back({0.46f, h - 0.1f});
    outline.push_back({0.42f, h - 0.065f});
    outline.push_back({0, h - 0.06f});
    return Lathe(outline, 48);
}

Mesh BuildDie()
{
    // A cube whose edges and corners are rounded off: each face a grid packed toward its edges, every vertex pushed out
    // to the rounded surface.
    constexpr int Grid = 18;
    constexpr float Round = 0.2f;
    Mesh mesh;
    const Vec3 axes[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const Vec3 n : axes) {
        const Vec3 u = std::abs(n.y) > 0.5f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
        const Vec3 v = Cross(n, u);
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (int i = 0; i <= Grid; ++i) {
            for (int j = 0; j <= Grid; ++j) {
                const float a = std::sin((static_cast<float>(i) / Grid * 2 - 1) * Pi / 2);
                const float b = std::sin((static_cast<float>(j) / Grid * 2 - 1) * Pi / 2);
                const Vec3 p = n + u * a + v * b;
                const Vec3 inner{std::clamp(p.x, Round - 1, 1 - Round), std::clamp(p.y, Round - 1, 1 - Round), std::clamp(p.z, Round - 1, 1 - Round)};
                const Vec3 normal = Normalize(p - inner);
                mesh.vertices.push_back({inner + normal * Round, normal, {a, b}});
            }
        }
        for (int i = 0; i < Grid; ++i) {
            for (int j = 0; j < Grid; ++j) {
                const auto at = [&](int di, int dj) { return base + static_cast<std::uint32_t>((i + di) * (Grid + 1) + j + dj); };
                // Counterclockwise from outside: u × v points along the normal.
                mesh.indices.insert(mesh.indices.end(), {at(0, 0), at(1, 0), at(1, 1), at(0, 0), at(1, 1), at(0, 1)});
            }
        }
    }
    return mesh;
}

} // namespace board
