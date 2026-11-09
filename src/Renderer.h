#pragma once

#include "BoardModel.h"
#include "Gl.h"
#include "MathTypes.h"
#include "Themes.h"

#include <array>
#include <initializer_list>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace render {

struct PieceDraw {
    math::Mat4 model = math::Mat4::Identity(); // with the size as its scale
    int kind = 0; // 0 a checker, 1 a die
    int side = 0; // White or Black
    int value = 0;
    float highlight = 0;
    float dim = 0; // a die already played
};

// A rectangle of the screen behind which the scene is blurred, in pixels from the top left.
struct FrostedPanel {
    float x = 0, y = 0, width = 0, height = 0, radius = 0, opacity = 1;
};

struct Scene {
    math::Mat4 view = math::Mat4::Identity();
    math::Mat4 projection = math::Mat4::Identity();
    math::Vec3 eye;
    float time = 0;
    std::vector<PieceDraw> pieces; // the checkers, then the dice
    // For each point as White counts them: x a place to move to, y a checker that may move, z the last move, w the hint.
    std::array<math::Vec4, 24> points{};
    math::Vec4 trays; // x White's tray as a place to move to, y Black's; z the hint into White's (above 0) or Black's
    std::vector<FrostedPanel> panels;
};

// A linked shader program, its sources taken from the executable's resources.
class Program {
public:
    struct Stage {
        gl::GLenum type;
        std::initializer_list<const wchar_t*> sources;
    };

    Program() = default;
    Program(std::string name, std::initializer_list<Stage> stages);
    // The GLSL version the shaders are compiled for, from the context's OpenGL version (46 for 4.6).
    static void SetVersion(int glVersion);

    gl::GLuint Id() const { return m_program.Id(); }
    void Use() const { gl::glUseProgram(m_program.Id()); }
    gl::GLint Location(const char* name) const;

    void Set(const char* name, int value) const;
    void Set(const char* name, float value) const;
    void Set(const char* name, math::Vec2 value) const;
    void Set(const char* name, math::Vec3 value) const;
    void Set(const char* name, math::Vec4 value) const;
    void Set(const char* name, const math::Mat4& value) const;
    void SetArray(const char* name, std::span<const math::Vec4> values) const;

private:
    std::string m_name;
    gl::ProgramObject m_program;
    mutable std::unordered_map<std::string, gl::GLint> m_locations;
};

// Draws the scene into the window: shadows, the lit scene in high dynamic range with multisampling, then bloom
// and tone mapping. The interface is drawn on top afterwards.
class Renderer {
public:
    explicit Renderer(int glVersion);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void SetTheme(const Theme& theme);
    void Resize(int width, int height);
    void Render(const Scene& scene);

private:
    struct GpuMesh {
        gl::VertexArray vertexArray;
        gl::Buffer vertices;
        gl::Buffer indices;
        gl::GLsizei count = 0;
        void Draw(int instances = 1) const;
    };

    static GpuMesh Upload(const board::Mesh& mesh);
    void CreateEnvironmentTextures();
    void BakeEnvironment();
    void UpdateLightView();
    void CreateScreenTargets();
    void UpdateFrameData(const Scene& scene);
    void UploadPieces(const Scene& scene);
    void SetPass(math::Vec3 eye, bool shadows);
    void BindLighting() const;
    void DrawShadowMap();
    void DrawPieces(const Program& program) const;
    void DrawMainPass(const Scene& scene);
    void SetMaterials(const Scene& scene) const;
    void PostProcess(const Scene& scene);

    int m_width = 0, m_height = 0;
    int m_samples = 4;
    Theme m_theme;
    math::Mat4 m_lightViewProjection;
    float m_shadowRadius = 1, m_shadowRange = 4;

    GpuMesh m_checker, m_die, m_field, m_wood, m_felt, m_table;
    gl::VertexArray m_emptyVertexArray;

    Program m_pieceProgram, m_boardProgram, m_shadowProgram, m_backgroundProgram;
    Program m_resolveProgram, m_bloomDownProgram, m_bloomUpProgram, m_compositeProgram;
    Program m_environmentProgram, m_irradianceProgram, m_prefilterProgram, m_brdfProgram;

    gl::Buffer m_frameData, m_passData;
    gl::Buffer m_pieceBuffer;
    int m_checkers = 0, m_dice = 0;

    gl::Texture m_shadowMap;
    gl::Framebuffer m_shadowFramebuffer;
    gl::Sampler m_shadowCompareSampler, m_shadowDepthSampler;
    gl::Texture m_environment, m_irradiance, m_prefiltered, m_brdf;
    int m_prefilteredLevels = 6;

    gl::Texture m_sceneColor, m_sceneDepth, m_resolved;
    gl::Framebuffer m_sceneFramebuffer, m_resolvedFramebuffer;
    static constexpr int BloomLevels = 6;
    std::array<gl::Texture, BloomLevels> m_bloom;
    std::array<gl::Framebuffer, BloomLevels> m_bloomFramebuffers;
    std::array<std::array<int, 2>, BloomLevels> m_bloomSizes{};
};

} // namespace render
