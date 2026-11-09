#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace text {

using namespace gl;

namespace {

// A device context drawing into a 32-bit top-down bitmap.
class Canvas {
public:
    Canvas(int width, int height) : m_width(width), m_height(height)
    {
        m_dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        m_bitmap = CreateDIBSection(m_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!m_dc || !m_bitmap || !bits)
            throw std::runtime_error("GDI could not create a bitmap for the text.");
        m_pixels = static_cast<std::uint32_t*>(bits);
        std::memset(m_pixels, 0, static_cast<std::size_t>(width) * height * 4);
        m_previous = SelectObject(m_dc, m_bitmap);
        SetBkMode(m_dc, TRANSPARENT);
        SetTextColor(m_dc, RGB(255, 255, 255));
    }

    ~Canvas()
    {
        SelectObject(m_dc, m_previous);
        DeleteObject(m_bitmap);
        DeleteDC(m_dc);
    }

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    HDC Dc() const { return m_dc; }

    // The coverage of the white text, one byte a pixel.
    std::vector<std::uint8_t> Coverage() const
    {
        GdiFlush();
        std::vector<std::uint8_t> coverage(static_cast<std::size_t>(m_width) * m_height);
        for (std::size_t i = 0; i < coverage.size(); ++i)
            coverage[i] = static_cast<std::uint8_t>((m_pixels[i] >> 8) & 0xFF);
        return coverage;
    }

private:
    int m_width, m_height;
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_previous = nullptr;
    std::uint32_t* m_pixels = nullptr;
};

class FontHandle {
public:
    FontHandle(const wchar_t* face, int height, int weight, int escapement = 0)
        : m_font(CreateFontW(-height, 0, escapement, escapement, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face))
    {
    }
    ~FontHandle() { DeleteObject(m_font); }
    FontHandle(const FontHandle&) = delete;
    FontHandle& operator=(const FontHandle&) = delete;
    HFONT Get() const { return m_font; }

private:
    HFONT m_font;
};

std::vector<char32_t> Codepoints()
{
    std::vector<char32_t> codepoints;
    for (char32_t c = 0x20; c < 0x7F; ++c)
        codepoints.push_back(c);
    for (char32_t c = 0x410; c <= 0x44F; ++c)
        codepoints.push_back(c);
    for (char32_t c : {0x401, 0x451, 0x2014, 0x2013, 0x2026, 0x2022, 0x00B7, 0x2190, 0x2192, 0x2191, 0x2193, 0x00D7, 0x00BD, 0x00AB, 0x00BB, 0x2116,
                       0x21B6, 0x21B7, 0x25B2, 0x25BC, 0x25C0, 0x25B6})
        codepoints.push_back(c);
    return codepoints;
}

bool HasGlyph(HDC dc, wchar_t character)
{
    WORD index = 0;
    return GetGlyphIndicesW(dc, &character, 1, &index, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && index != 0xFFFF;
}

} // namespace

char32_t NextCodepoint(std::string_view text, std::size_t& index)
{
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char first = byte(index++);
    if (first < 0x80)
        return first;
    int extra = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;
    char32_t codepoint = first & (0x3F >> extra);
    while (extra-- > 0 && index < text.size())
        codepoint = (codepoint << 6) | (byte(index++) & 0x3F);
    return codepoint;
}

Font::Font(const wchar_t* face, float pixelHeight, int weight)
{
    const int height = std::max(6, static_cast<int>(std::lround(pixelHeight)));
    FontHandle font(face, height, weight);
    FontHandle symbols(L"Segoe UI Symbol", height, weight);

    struct Item {
        char32_t codepoint;
        bool symbol;
        int advance;
        int x = 0, y = 0;
    };
    std::vector<Item> items;

    HDC measure = CreateCompatibleDC(nullptr);
    SelectObject(measure, symbols.Get());
    TEXTMETRICW symbolMetrics{};
    GetTextMetricsW(measure, &symbolMetrics);
    SelectObject(measure, font.Get());
    TEXTMETRICW metrics{};
    GetTextMetricsW(measure, &metrics);
    m_lineHeight = static_cast<float>(metrics.tmHeight + metrics.tmExternalLeading);
    m_ascent = static_cast<float>(metrics.tmAscent);

    for (char32_t codepoint : Codepoints()) {
        const auto character = static_cast<wchar_t>(codepoint);
        bool symbol = false;
        SelectObject(measure, font.Get());
        if (codepoint >= 0x80 && !HasGlyph(measure, character)) {
            SelectObject(measure, symbols.Get());
            if (!HasGlyph(measure, character))
                continue;
            symbol = true;
        }
        SIZE size{};
        GetTextExtentPoint32W(measure, &character, 1, &size);
        items.push_back({codepoint, symbol, static_cast<int>(size.cx)});
    }
    DeleteDC(measure);

    // Cells in rows, with room for overhanging strokes.
    const int pad = std::max(2, height / 6);
    const int ascent = std::max(metrics.tmAscent, symbolMetrics.tmAscent);
    const int cellHeight = ascent + std::max(metrics.tmDescent, symbolMetrics.tmDescent) + 2 * pad;
    int atlasWidth = 256, atlasHeight = 0;
    for (;; atlasWidth *= 2) {
        int x = 0, y = 0;
        for (Item& item : items) {
            const int cellWidth = item.advance + 2 * pad;
            if (x + cellWidth > atlasWidth) {
                x = 0;
                y += cellHeight;
            }
            item.x = x;
            item.y = y;
            x += cellWidth;
        }
        atlasHeight = y + cellHeight;
        if (atlasHeight <= atlasWidth)
            break;
    }

    Canvas canvas(atlasWidth, atlasHeight);
    SetTextAlign(canvas.Dc(), TA_LEFT | TA_BASELINE);
    for (const Item& item : items) {
        SelectObject(canvas.Dc(), item.symbol ? symbols.Get() : font.Get());
        const auto character = static_cast<wchar_t>(item.codepoint);
        TextOutW(canvas.Dc(), item.x + pad, item.y + pad + ascent, &character, 1);
    }
    const std::vector<std::uint8_t> coverage = canvas.Coverage();

    m_texture = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_texture.Id(), 1, GL_R8, atlasWidth, atlasHeight);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_texture.Id(), 0, 0, 0, atlasWidth, atlasHeight, GL_RED, GL_UNSIGNED_BYTE, coverage.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTextureParameteri(m_texture.Id(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_texture.Id(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_texture.Id(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_texture.Id(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const float w = static_cast<float>(atlasWidth), h = static_cast<float>(atlasHeight);
    for (const Item& item : items) {
        const int cellWidth = item.advance + 2 * pad;
        Glyph glyph;
        glyph.u0 = static_cast<float>(item.x) / w;
        glyph.v0 = static_cast<float>(item.y) / h;
        glyph.u1 = static_cast<float>(item.x + cellWidth) / w;
        glyph.v1 = static_cast<float>(item.y + cellHeight) / h;
        glyph.width = static_cast<float>(cellWidth);
        glyph.height = static_cast<float>(cellHeight);
        glyph.left = -static_cast<float>(pad);
        glyph.top = static_cast<float>(metrics.tmAscent - ascent - pad);
        glyph.advance = static_cast<float>(item.advance);
        m_glyphs.emplace(item.codepoint, glyph);
    }
    m_fallback = m_glyphs.contains(U'?') ? m_glyphs.at(U'?') : Glyph{};
}

const Glyph& Font::Find(char32_t codepoint) const
{
    const auto found = m_glyphs.find(codepoint);
    return found != m_glyphs.end() ? found->second : m_fallback;
}

float Font::Measure(std::string_view utf8) const
{
    float width = 0;
    for (std::size_t i = 0; i < utf8.size();)
        width += Find(NextCodepoint(utf8, i)).advance;
    return width;
}

} // namespace text
