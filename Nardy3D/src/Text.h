#pragma once

#include "Gl.h"

#include <string_view>
#include <unordered_map>

// Fonts drawn by GDI into textures, and text measured and laid out from them.
namespace text {

struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // in the atlas
    float width = 0, height = 0;          // of the quad, in pixels
    float left = 0, top = 0;              // from the pen and the line's top to the quad
    float advance = 0;
};

class Font {
public:
    // A font of the given pixel height, with the Latin and Cyrillic letters and a few symbols.
    Font(const wchar_t* face, float pixelHeight, int weight);

    const Glyph& Find(char32_t codepoint) const;
    float LineHeight() const { return m_lineHeight; }
    float Ascent() const { return m_ascent; }
    float Measure(std::string_view utf8) const;
    gl::GLuint Texture() const { return m_texture.Id(); }

private:
    std::unordered_map<char32_t, Glyph> m_glyphs;
    Glyph m_fallback;
    float m_lineHeight = 0;
    float m_ascent = 0;
    gl::Texture m_texture;
};

// Decodes UTF-8 one code point at a time.
char32_t NextCodepoint(std::string_view text, std::size_t& index);

} // namespace text
