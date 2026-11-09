#include "Themes.h"

namespace {

// The lamp over the board, and a dim room round it.
void Lamp(Theme& t, math::Vec3 radiance, math::Vec3 fill)
{
    t.panels[0] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {0.26f, 0.2f}, .corner = 0.45f, .softness = 0.1f, .radiance = radiance * 1.6f};
    t.panels[1] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {1.4f, 0.9f}, .corner = 0.3f, .softness = 0.6f, .radiance = radiance * 0.08f};
    t.panels[2] = {.direction = {0.9f, 0.25f, 0.35f}, .halfSize = {0.8f, 0.3f}, .corner = 0.1f, .softness = 0.3f, .radiance = fill};
    t.panels[3] = {.direction = {-0.9f, 0.25f, -0.35f}, .halfSize = {0.8f, 0.3f}, .corner = 0.1f, .softness = 0.3f, .radiance = fill * 0.7f};
    t.panels[4] = {.direction = {0.1f, 0.3f, 0.95f}, .halfSize = {1.2f, 0.25f}, .corner = 0.1f, .softness = 0.4f, .radiance = fill * 0.5f};
    t.panelCount = 5;
}

// A walnut box with a light field, dark and red points, ivory and ebony checkers.
Theme Classic()
{
    Theme t{
        .nameEnglish = "Classic",
        .nameRussian = "Классика",
        .field = {0.42f, 0.25f, 0.11f},
        .fieldRoughness = 0.4f,
        .fieldSeed = 0.37f,
        .pointA = {0.05f, 0.025f, 0.013f},
        .pointB = {0.3f, 0.055f, 0.025f},
        .line = {0.95f, 0.7f, 0.35f},
        .wood = {0.11f, 0.05f, 0.022f},
        .woodSeed = 0.61f,
        .felt = {0.02f, 0.075f, 0.035f},
        .table = {0.07f, 0.04f, 0.022f},
        .tableRoughness = 0.4f,
        .checkers = {CheckerLook{.color = {0.72f, 0.6f, 0.42f}, .roughness = 0.3f, .pattern = 1, .ornament = 0.5f},
                     CheckerLook{.color = {0.035f, 0.022f, 0.016f}, .roughness = 0.25f, .pattern = 1, .ornament = 0.5f}},
        .die = {0.82f, 0.76f, 0.63f},
        .dieRoughness = 0.3f,
        .pip = {0.02f, 0.018f, 0.016f},
        .keyDirection = {0.15f, 1.0f, 0.3f},
        .keyRadiance = {4.6f, 4.35f, 3.9f},
        .ceiling = {0.012f, 0.011f, 0.01f},
        .horizon = {0.035f, 0.028f, 0.02f},
        .ground = {0.012f, 0.01f, 0.008f},
        .diffuseLight = 0.85f,
        .background = 0.4f,
        .fog = {0.012f, 0.01f, 0.008f},
        .exposure = 0.95f,
        .accent = {0.3f, 0.8f, 1.0f},
        .hint = {0.5f, 1.0f, 0.35f},
    };
    Lamp(t, {8.6f, 8.0f, 7.0f}, {0.13f, 0.11f, 0.09f});
    return t;
}

// Red leather with cream and green points and a gold line, a lacquered box, bone and onyx, a carpet under it.
Theme Oriental()
{
    Theme t{
        .nameEnglish = "Oriental",
        .nameRussian = "Восток",
        .field = {0.16f, 0.025f, 0.02f},
        .fieldRoughness = 0.55f,
        .fieldPattern = 2,
        .fieldSeed = 0.2f,
        .fieldClearcoat = 0.2f,
        .pointA = {0.6f, 0.48f, 0.3f},
        .pointB = {0.015f, 0.07f, 0.045f},
        .pointRoughness = 0.5f,
        .lineWidth = 0.0008f,
        .line = {1.0f, 0.75f, 0.32f},
        .wood = {0.075f, 0.022f, 0.012f},
        .woodRoughness = 0.25f,
        .woodClearcoat = 1.0f,
        .woodSeed = 0.18f,
        .felt = {0.12f, 0.015f, 0.015f},
        .table = {0.09f, 0.018f, 0.016f},
        .tableRoughness = 0.9f,
        .tablePattern = 1,
        .checkers = {CheckerLook{.color = {0.78f, 0.71f, 0.57f}, .roughness = 0.35f, .clearcoat = 0.5f, .pattern = 3, .ornament = 0.8f},
                     CheckerLook{.color = {0.03f, 0.035f, 0.04f}, .roughness = 0.12f, .clearcoat = 0.9f, .pattern = 2, .ornament = 0.8f}},
        .die = {0.85f, 0.79f, 0.66f},
        .dieRoughness = 0.35f,
        .pip = {0.25f, 0.03f, 0.02f},
        .keyDirection = {0.2f, 1.0f, 0.25f},
        .keyRadiance = {4.7f, 4.2f, 3.5f},
        .ceiling = {0.012f, 0.009f, 0.006f},
        .horizon = {0.04f, 0.026f, 0.014f},
        .ground = {0.014f, 0.008f, 0.006f},
        .diffuseLight = 0.85f,
        .background = 0.4f,
        .fog = {0.014f, 0.009f, 0.006f},
        .exposure = 1.0f,
        .accent = {1.0f, 0.75f, 0.3f},
        .hint = {0.25f, 0.95f, 0.85f},
    };
    Lamp(t, {8.6f, 7.6f, 6.2f}, {0.15f, 0.1f, 0.06f});
    return t;
}

// Green felt with cream and burgundy points, black lacquer, white and black checkers, red precision dice.
Theme Tournament()
{
    Theme t{
        .nameEnglish = "Tournament",
        .nameRussian = "Турнир",
        .field = {0.02f, 0.09f, 0.045f},
        .fieldRoughness = 0.9f,
        .fieldPattern = 1,
        .fieldClearcoat = 0,
        .pointA = {0.62f, 0.56f, 0.44f},
        .pointB = {0.2f, 0.02f, 0.03f},
        .pointRoughness = 0.85f,
        .lineWidth = 0,
        .line = {0.6f, 0.6f, 0.6f},
        .wood = {0.012f, 0.012f, 0.014f},
        .woodRoughness = 0.25f,
        .woodClearcoat = 0.7f,
        .woodPattern = 1,
        .felt = {0.02f, 0.08f, 0.04f},
        .table = {0.07f, 0.07f, 0.075f},
        .tableRoughness = 0.45f,
        .tablePattern = 2,
        .tableSeed = 0.4f,
        .checkers = {CheckerLook{.color = {0.8f, 0.78f, 0.72f}, .roughness = 0.2f, .clearcoat = 0.9f, .ornament = 0.25f},
                     CheckerLook{.color = {0.02f, 0.02f, 0.024f}, .roughness = 0.18f, .clearcoat = 0.9f, .ornament = 0.25f}},
        .die = {0.45f, 0.02f, 0.025f},
        .dieRoughness = 0.12f,
        .pip = {0.85f, 0.85f, 0.85f},
        .keyDirection = {0.1f, 1.0f, 0.2f},
        .keyRadiance = {5.0f, 5.0f, 5.1f},
        .keySoftness = 0.2f,
        .ceiling = {0.012f, 0.013f, 0.016f},
        .horizon = {0.03f, 0.032f, 0.04f},
        .ground = {0.01f, 0.01f, 0.012f},
        .diffuseLight = 0.9f,
        .background = 0.4f,
        .fog = {0.01f, 0.011f, 0.014f},
        .exposure = 1.0f,
        .accent = {0.4f, 0.8f, 1.0f},
        .hint = {1.0f, 0.7f, 0.25f},
    };
    Lamp(t, {9.2f, 9.2f, 9.4f}, {0.1f, 0.11f, 0.14f});
    return t;
}

// Dark leather and slate points with a silver line, pearl and black stone, a cool dim room.
Theme Night()
{
    Theme t{
        .nameEnglish = "Night",
        .nameRussian = "Ночь",
        .field = {0.018f, 0.022f, 0.03f},
        .fieldRoughness = 0.5f,
        .fieldPattern = 2,
        .fieldSeed = 0.8f,
        .fieldClearcoat = 0.3f,
        .pointA = {0.05f, 0.1f, 0.17f},
        .pointB = {0.16f, 0.17f, 0.19f},
        .pointRoughness = 0.4f,
        .line = {0.75f, 0.8f, 0.85f},
        .wood = {0.02f, 0.02f, 0.025f},
        .woodRoughness = 0.2f,
        .woodClearcoat = 1.0f,
        .woodPattern = 1,
        .felt = {0.012f, 0.03f, 0.05f},
        .table = {0.012f, 0.014f, 0.02f},
        .tableRoughness = 0.9f,
        .tablePattern = 1,
        .checkers = {CheckerLook{.color = {0.7f, 0.73f, 0.78f}, .roughness = 0.15f, .clearcoat = 1.0f, .pattern = 2, .ornament = 0.6f},
                     CheckerLook{.color = {0.012f, 0.013f, 0.016f}, .roughness = 0.12f, .clearcoat = 1.0f, .pattern = 2, .ornament = 0.6f}},
        .die = {0.03f, 0.03f, 0.035f},
        .dieRoughness = 0.15f,
        .pip = {0.3f, 0.85f, 1.0f},
        .keyDirection = {0.08f, 1.0f, 0.18f},
        .keyRadiance = {3.6f, 3.9f, 4.4f},
        .keySoftness = 0.12f,
        .ceiling = {0.004f, 0.005f, 0.01f},
        .horizon = {0.01f, 0.015f, 0.03f},
        .ground = {0.003f, 0.004f, 0.006f},
        .diffuseLight = 1.0f,
        .reflectedLight = 1.1f,
        .background = 0.5f,
        .fog = {0.004f, 0.006f, 0.012f},
        .exposure = 1.15f,
        .bloom = 0.07f,
        .vignette = 0.55f,
        .accent = {0.2f, 0.85f, 1.0f},
        .hint = {0.85f, 0.5f, 1.0f},
    };
    Lamp(t, {6.8f, 7.4f, 8.4f}, {0.05f, 0.08f, 0.16f});
    t.panels[5] = {.direction = {-0.3f, 0.2f, 0.93f}, .halfSize = {1.0f, 0.03f}, .corner = 0.01f, .softness = 0.02f, .radiance = {1.0f, 3.0f, 4.0f}};
    t.panelCount = 6;
    return t;
}

} // namespace

const std::array<Theme, ThemeCount>& Themes()
{
    static const std::array<Theme, ThemeCount> themes{Classic(), Oriental(), Tournament(), Night()};
    return themes;
}
