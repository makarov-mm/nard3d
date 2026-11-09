#pragma once

#include "MathTypes.h"

#include <array>
#include <string_view>

// The looks of the board, the checkers, the dice, the table under them and the lamp over it.
struct StudioPanel {
    math::Vec3 direction; // toward the panel
    float roll = 0;
    math::Vec2 halfSize;  // as tangents of the angles
    float corner = 0.05f;
    float softness = 0.05f;
    math::Vec3 radiance;
    float falloff = 0.3f;
};

struct CheckerLook {
    math::Vec3 color;
    float roughness = 0.3f;
    float clearcoat = 0.7f;
    float pattern = 0;    // 0 plain, 1 wood, 2 stone, 3 bone
    float ornament = 0.6f; // how deep the rosette is carved
};

struct Theme {
    std::string_view nameEnglish;
    std::string_view nameRussian;

    // The fields and the points inlaid in them.
    math::Vec3 field;
    float fieldRoughness = 0.45f;
    float fieldPattern = 0; // 0 wood, 1 felt, 2 leather
    float fieldSeed = 0.3f;
    float fieldClearcoat = 0.5f;
    math::Vec3 pointA, pointB;
    float pointRoughness = 0.45f;
    float lineWidth = 0.0006f;
    float lineMetallic = 1;
    math::Vec3 line;
    // The rims, the bar and the tray.
    math::Vec3 wood;
    float woodRoughness = 0.3f;
    float woodClearcoat = 0.8f;
    float woodPattern = 0; // 0 wood, 1 lacquer
    float woodSeed = 0.5f;
    math::Vec3 felt;
    // The table under the board.
    math::Vec3 table;
    float tableRoughness = 0.5f;
    float tablePattern = 0; // 0 wood, 1 cloth, 2 stone
    float tableSeed = 0.2f;

    std::array<CheckerLook, 2> checkers; // White's and Black's
    math::Vec3 die;
    float dieRoughness = 0.25f;
    math::Vec3 pip;

    // The light.
    math::Vec3 keyDirection;
    math::Vec3 keyRadiance;
    float keySoftness = 0.15f; // tangent of the light's angular radius
    math::Vec3 ceiling, horizon, ground;
    std::array<StudioPanel, 6> panels{};
    int panelCount = 0;
    float diffuseLight = 1;
    float reflectedLight = 1;
    float background = 0.5f;
    math::Vec3 fog;
    float exposure = 1;
    float bloom = 0.04f;
    float vignette = 0.45f;

    math::Vec3 accent; // where a checker may go, the chosen checker and the interface
    math::Vec3 hint;   // the computer's advice
};

constexpr int ThemeCount = 4;

const std::array<Theme, ThemeCount>& Themes();
