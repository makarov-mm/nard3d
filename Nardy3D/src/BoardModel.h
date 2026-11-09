#pragma once

#include "MathTypes.h"

#include <cstdint>
#include <vector>

// The board's measurements and the meshes of the scene: the board in two halves with its rims, the middle bar and the
// tray for the checkers borne off, the table under it, the checker and the die. The playing surface is the plane
// y = 0 with x to the right and z toward White, who sits at the near side. Point w (0 to 23, as White counts them)
// lies along the near edge for w < 12, from the right, and along the far edge from the left for the rest.
namespace board {

constexpr float PointWidth = 0.042f;
constexpr float PointLength = 0.2f;         // from the edge to the tip
constexpr float HalfDepth = 0.23f;          // from the middle to the near edge of the field
constexpr float BarHalf = 0.028f;           // half the width of the middle bar
constexpr float RimWidth = 0.032f;
constexpr float RimHeight = 0.022f;
constexpr float Base = 0.03f;               // the board's thickness under the field
constexpr float TrayWidth = 0.058f;         // the tray beside the right half
constexpr float CheckerRadius = 0.0192f;
constexpr float CheckerHeight = 0.0088f;
constexpr float CheckerStep = 0.0396f;      // between the middles of neighbours along a point
constexpr int RowLength = 5;                // checkers along a point before they stack
constexpr float DieSize = 0.0175f;          // half the edge of a die
constexpr float FieldRight = BarHalf + 6 * PointWidth; // the field's outer edge

// The middle of point w's base on the field, and which way along z its tip lies (+1 or -1).
math::Vec3 PointBase(int w);
float PointToward(int w);
// Where the index-th checker on point w lies: in a row toward the tip, then in towers.
math::Vec3 CheckerSpot(int w, int index);
// On the middle bar: White's (side 0) toward the far side, where they enter; Black's toward the near side.
math::Vec3 BarSpot(int side, int index);
// Standing on edge in the tray: White's in the near compartment, Black's in the far one.
math::Vec3 OffSpot(int side, int index);
// Where a die thrown by `side` comes to rest: White's in the right half, Black's in the left.
math::Vec3 DieSpot(int side, int index);
// The point under a place on the field, or -1.
int PointAt(math::Vec2 xz);

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct BoardMeshes {
    Mesh field; // the two playing surfaces; the shader draws the points on them
    Mesh wood;  // the rims, the middle bar, the base and the tray
    Mesh felt;  // the floors of the tray's compartments
    Mesh table; // the table the board stands on
};

BoardMeshes BuildBoard();
// A checker of radius 1 lying on y = 0, its top at CheckerHeight / CheckerRadius. uv.y is the distance along its outline.
Mesh BuildChecker();
// A die with rounded edges, its faces at ±1.
Mesh BuildDie();

} // namespace board
