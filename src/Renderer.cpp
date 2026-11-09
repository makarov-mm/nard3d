#include "Renderer.h"

#include "Resources.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <utility>

namespace render {

using namespace gl;
using namespace math;

namespace {

int g_glslVersion = 460;

constexpr int ShadowSize = 2048;
constexpr int EnvironmentSize = 512;
constexpr int IrradianceSize = 32;
constexpr int PrefilteredSize = 256;
constexpr int BrdfSize = 128;

// std140 layouts of the uniform blocks in common.glsl.
struct FrameBlock {
    Mat4 viewProjection;
    Mat4 inverseViewProjection;
    Mat4 lightViewProjection;
    Vec4 cameraPosition;
    Vec4 lightDirection;
    Vec4 lightColor;
    Vec4 shadowParams;
    Vec4 viewport;
    Vec4 environment;
    Vec4 accent;
    Vec4 fog;
};

struct PassBlock {
    Vec4 eye;
};

// A checker or a die in the std430 buffer of piece.vert.
struct PieceInstance {
    Mat4 model;
    Vec4 params; // kind, side, number, highlight
    Vec4 extra;  // darkened
};
static_assert(sizeof(PieceInstance) == 96);

constexpr int MaxPieces = 32;

Vec4 V4(Vec3 v, float w) { return {v.x, v.y, v.z, w}; }

int LevelsFor(int size)
{
    int levels = 1;
    while ((size >> levels) > 0)
        ++levels;
    return levels;
}

void Dispatch(int width, int height, int layers)
{
    glDispatchCompute(static_cast<GLuint>((width + 7) / 8), static_cast<GLuint>((height + 7) / 8), static_cast<GLuint>(layers));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void SetFilter(GLuint texture, GLenum minFilter, GLenum magFilter, GLenum wrap)
{
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrap));
}

void CheckFramebuffer(const Framebuffer& framebuffer, const char* name)
{
    if (glCheckNamedFramebufferStatus(framebuffer.Id(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error(std::format("The {} framebuffer is not complete.", name));
}

} // namespace

// Program ------------------------------------------------------------------------------------------

void Program::SetVersion(int glVersion)
{
    g_glslVersion = glVersion >= 46 ? 460 : 450;
}

Program::Program(std::string name, std::initializer_list<Stage> stages) : m_name(std::move(name))
{
    const GLuint program = glCreateProgram();
    m_program = ProgramObject(program);
    std::vector<GLuint> shaders;
    for (const Stage& stage : stages) {
        std::string source = std::format("#version {} core\n", g_glslVersion);
        int index = 0;
        for (const wchar_t* resource : stage.sources) {
            source += std::format("#line 1 {}\n", index++);
            source += resources::Text(resource);
            source += '\n';
        }
        const GLuint shader = glCreateShader(stage.type);
        const char* text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            std::string log(4096, '\0');
            GLsizei length = 0;
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &length, log.data());
            log.resize(static_cast<std::size_t>(length));
            glDeleteShader(shader);
            for (GLuint attached : shaders)
                glDeleteShader(attached);
            throw std::runtime_error(std::format("The {} shader does not compile:\n{}", m_name, log));
        }
        glAttachShader(program, shader);
        shaders.push_back(shader);
    }
    glLinkProgram(program);
    for (GLuint shader : shaders)
        glDeleteShader(shader);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::string log(4096, '\0');
        GLsizei length = 0;
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &length, log.data());
        log.resize(static_cast<std::size_t>(length));
        throw std::runtime_error(std::format("The {} shaders do not link:\n{}", m_name, log));
    }
}

GLint Program::Location(const char* name) const
{
    const auto found = m_locations.find(name);
    if (found != m_locations.end())
        return found->second;
    const GLint location = glGetUniformLocation(m_program.Id(), name);
    m_locations.emplace(name, location);
    return location;
}

void Program::Set(const char* name, int value) const { glProgramUniform1i(Id(), Location(name), value); }
void Program::Set(const char* name, float value) const { glProgramUniform1f(Id(), Location(name), value); }
void Program::Set(const char* name, Vec2 value) const { glProgramUniform2f(Id(), Location(name), value.x, value.y); }
void Program::Set(const char* name, Vec3 value) const { glProgramUniform3f(Id(), Location(name), value.x, value.y, value.z); }
void Program::Set(const char* name, Vec4 value) const { glProgramUniform4f(Id(), Location(name), value.x, value.y, value.z, value.w); }
void Program::Set(const char* name, const Mat4& value) const { glProgramUniformMatrix4fv(Id(), Location(name), 1, GL_FALSE, value.Data()); }

void Program::SetArray(const char* name, std::span<const Vec4> values) const
{
    if (!values.empty())
        glProgramUniform4fv(Id(), Location(name), static_cast<GLsizei>(values.size()), &values.front().x);
}

// Meshes -------------------------------------------------------------------------------------------

void Renderer::GpuMesh::Draw(int instances) const
{
    if (count == 0 || instances <= 0)
        return;
    glBindVertexArray(vertexArray.Id());
    if (instances == 1)
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    else
        glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instances);
}

Renderer::GpuMesh Renderer::Upload(const board::Mesh& mesh)
{
    GpuMesh gpu;
    if (mesh.indices.empty())
        return gpu;
    gpu.vertices = CreateBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(board::Vertex));
    gpu.indices = CreateBuffer(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    gpu.count = static_cast<GLsizei>(mesh.indices.size());
    GLuint array = 0;
    glCreateVertexArrays(1, &array);
    gpu.vertexArray = VertexArray(array);
    glVertexArrayVertexBuffer(array, 0, gpu.vertices.Id(), 0, sizeof(board::Vertex));
    glVertexArrayElementBuffer(array, gpu.indices.Id());
    const GLuint offsets[3] = {offsetof(board::Vertex, position), offsetof(board::Vertex, normal), offsetof(board::Vertex, uv)};
    const GLint sizes[3] = {3, 3, 2};
    for (GLuint attribute = 0; attribute < 3; ++attribute) {
        glEnableVertexArrayAttrib(array, attribute);
        glVertexArrayAttribFormat(array, attribute, sizes[attribute], GL_FLOAT, GL_FALSE, offsets[attribute]);
        glVertexArrayAttribBinding(array, attribute, 0);
    }
    return gpu;
}

// Setup --------------------------------------------------------------------------------------------

Renderer::Renderer(int glVersion)
{
    Program::SetVersion(glVersion);

    using Stage = Program::Stage;
    m_pieceProgram = Program("piece", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"PIECE_VERT"}},
                                       Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"LIGHTING_GLSL", L"PIECE_FRAG"}}});
    m_boardProgram = Program("board", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"BOARD_VERT"}},
                                       Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"LIGHTING_GLSL", L"BOARD_FRAG"}}});
    m_shadowProgram = Program("shadow", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"SHADOW_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"SHADOW_FRAG"}}});
    m_backgroundProgram = Program("background", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BACKGROUND_FRAG"}}});
    m_resolveProgram = Program("resolve", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"RESOLVE_FRAG"}}});
    m_bloomDownProgram = Program("bloom", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BLOOM_DOWN_FRAG"}}});
    m_bloomUpProgram = Program("bloom", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BLOOM_UP_FRAG"}}});
    m_compositeProgram = Program("composite", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"COMPOSITE_FRAG"}}});
    m_environmentProgram = Program("environment", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"ENVIRONMENT_COMP"}}});
    m_irradianceProgram = Program("irradiance", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"IRRADIANCE_COMP"}}});
    m_prefilterProgram = Program("prefilter", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"PREFILTER_COMP"}}});
    m_brdfProgram = Program("brdf", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"BRDF_COMP"}}});

    const board::BoardMeshes meshes = board::BuildBoard();
    m_field = Upload(meshes.field);
    m_wood = Upload(meshes.wood);
    m_felt = Upload(meshes.felt);
    m_table = Upload(meshes.table);
    m_checker = Upload(board::BuildChecker());
    m_die = Upload(board::BuildDie());
    GLuint emptyArray = 0;
    glCreateVertexArrays(1, &emptyArray);
    m_emptyVertexArray = VertexArray(emptyArray);

    m_frameData = CreateBuffer(nullptr, sizeof(FrameBlock), GL_DYNAMIC_STORAGE_BIT);
    m_passData = CreateBuffer(nullptr, sizeof(PassBlock), GL_DYNAMIC_STORAGE_BIT);
    m_pieceBuffer = CreateBuffer(nullptr, sizeof(PieceInstance) * MaxPieces, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_frameData.Id());
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_passData.Id());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pieceBuffer.Id());

    // The shadow map, read both with comparison (for filtering) and without (to find the casters).
    m_shadowMap = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_shadowMap.Id(), 1, GL_DEPTH_COMPONENT32F, ShadowSize, ShadowSize);
    m_shadowFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_shadowFramebuffer.Id(), GL_DEPTH_ATTACHMENT, m_shadowMap.Id(), 0);
    glNamedFramebufferDrawBuffer(m_shadowFramebuffer.Id(), GL_NONE);
    CheckFramebuffer(m_shadowFramebuffer, "shadow");
    GLuint samplers[2] = {};
    glCreateSamplers(2, samplers);
    m_shadowCompareSampler = Sampler(samplers[0]);
    m_shadowDepthSampler = Sampler(samplers[1]);
    for (GLuint sampler : samplers) {
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(samplers[0], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(samplers[0], GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    CreateEnvironmentTextures();

    GLint maxSamples = 4, maxColorSamples = 4, maxDepthSamples = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorSamples);
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthSamples);
    m_samples = std::max(1, std::min({4, maxSamples, maxColorSamples, maxDepthSamples}));
}

Renderer::~Renderer() = default;

void Renderer::CreateEnvironmentTextures()
{
    auto cube = [](int size, int levels) {
        Texture texture = CreateTexture(GL_TEXTURE_CUBE_MAP);
        glTextureStorage2D(texture.Id(), levels, GL_RGBA16F, size, size);
        SetFilter(texture.Id(), levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        return texture;
    };
    m_environment = cube(EnvironmentSize, LevelsFor(EnvironmentSize));
    m_irradiance = cube(IrradianceSize, 1);
    m_prefiltered = cube(PrefilteredSize, m_prefilteredLevels);

    m_brdf = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_brdf.Id(), 1, GL_RG16F, BrdfSize, BrdfSize);
    SetFilter(m_brdf.Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    glUseProgram(m_brdfProgram.Id());
    glBindImageTexture(0, m_brdf.Id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    Dispatch(BrdfSize, BrdfSize, 1);
}

void Renderer::BakeEnvironment()
{
    const Program& environment = m_environmentProgram;
    environment.Set("uCeiling", m_theme.ceiling);
    environment.Set("uHorizon", m_theme.horizon);
    environment.Set("uFloor", m_theme.ground);
    std::array<Vec4, 8> directions{}, shapes{}, colors{};
    for (int i = 0; i < m_theme.panelCount; ++i) {
        const StudioPanel& panel = m_theme.panels[static_cast<std::size_t>(i)];
        directions[static_cast<std::size_t>(i)] = V4(panel.direction, panel.roll);
        shapes[static_cast<std::size_t>(i)] = {panel.halfSize.x, panel.halfSize.y, panel.corner, panel.softness};
        colors[static_cast<std::size_t>(i)] = V4(panel.radiance, panel.falloff);
    }
    environment.SetArray("uPanelDirection", directions);
    environment.SetArray("uPanelShape", shapes);
    environment.SetArray("uPanelColor", colors);
    environment.Set("uPanelCount", m_theme.panelCount);
    environment.Use();
    glBindImageTexture(0, m_environment.Id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    Dispatch(EnvironmentSize, EnvironmentSize, 6);
    glGenerateTextureMipmap(m_environment.Id());

    glBindTextureUnit(0, m_environment.Id());
    m_irradianceProgram.Set("uSourceLod", 4.0f);
    m_irradianceProgram.Use();
    glBindImageTexture(0, m_irradiance.Id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    Dispatch(IrradianceSize, IrradianceSize, 6);

    m_prefilterProgram.Set("uSourceSize", static_cast<float>(EnvironmentSize));
    m_prefilterProgram.Use();
    for (int level = 0; level < m_prefilteredLevels; ++level) {
        const int size = PrefilteredSize >> level;
        m_prefilterProgram.Set("uRoughness", static_cast<float>(level) / static_cast<float>(m_prefilteredLevels - 1));
        glBindImageTexture(0, m_prefiltered.Id(), level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        Dispatch(size, size, 6);
    }
}

void Renderer::SetTheme(const Theme& theme)
{
    m_theme = theme;
    UpdateLightView();
    BakeEnvironment();
}

// The key light's view: a box round the board.
void Renderer::UpdateLightView()
{
    m_shadowRadius = 0.62f;
    m_shadowRange = 4;
    const Vec3 toLight = Normalize(m_theme.keyDirection);
    const Vec3 up = std::abs(toLight.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Vec3 center{0.03f, 0, 0};
    const Mat4 view = Mat4::LookAt(center + toLight * 2.0f, center, up);
    const Mat4 projection = Mat4::Orthographic(-m_shadowRadius, m_shadowRadius, -m_shadowRadius, m_shadowRadius, 0.5f, 0.5f + m_shadowRange);
    m_lightViewProjection = projection * view;
}

void Renderer::Resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;
    CreateScreenTargets();
}

void Renderer::CreateScreenTargets()
{
    m_sceneColor = CreateTexture(GL_TEXTURE_2D_MULTISAMPLE);
    glTextureStorage2DMultisample(m_sceneColor.Id(), m_samples, GL_RGBA16F, m_width, m_height, GL_TRUE);
    m_sceneDepth = CreateTexture(GL_TEXTURE_2D_MULTISAMPLE);
    glTextureStorage2DMultisample(m_sceneDepth.Id(), m_samples, GL_DEPTH_COMPONENT32F, m_width, m_height, GL_TRUE);
    m_sceneFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_sceneFramebuffer.Id(), GL_COLOR_ATTACHMENT0, m_sceneColor.Id(), 0);
    glNamedFramebufferTexture(m_sceneFramebuffer.Id(), GL_DEPTH_ATTACHMENT, m_sceneDepth.Id(), 0);
    CheckFramebuffer(m_sceneFramebuffer, "scene");

    m_resolved = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_resolved.Id(), 1, GL_RGBA16F, m_width, m_height);
    SetFilter(m_resolved.Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    m_resolvedFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_resolvedFramebuffer.Id(), GL_COLOR_ATTACHMENT0, m_resolved.Id(), 0);
    CheckFramebuffer(m_resolvedFramebuffer, "resolve");

    for (std::size_t level = 0; level < BloomLevels; ++level) {
        const int width = std::max(1, m_width >> (level + 1)), height = std::max(1, m_height >> (level + 1));
        m_bloomSizes[level] = {width, height};
        m_bloom[level] = CreateTexture(GL_TEXTURE_2D);
        glTextureStorage2D(m_bloom[level].Id(), 1, GL_R11F_G11F_B10F, width, height);
        SetFilter(m_bloom[level].Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        m_bloomFramebuffers[level] = CreateFramebuffer();
        glNamedFramebufferTexture(m_bloomFramebuffers[level].Id(), GL_COLOR_ATTACHMENT0, m_bloom[level].Id(), 0);
        CheckFramebuffer(m_bloomFramebuffers[level], "bloom");
    }
}

// Frame --------------------------------------------------------------------------------------------

void Renderer::UpdateFrameData(const Scene& scene)
{
    FrameBlock block;
    block.viewProjection = scene.projection * scene.view;
    block.inverseViewProjection = block.viewProjection.Inverse();
    block.lightViewProjection = m_lightViewProjection;
    block.cameraPosition = V4(scene.eye, scene.time);
    block.lightDirection = V4(Normalize(m_theme.keyDirection), 2 * m_shadowRadius / ShadowSize);
    block.lightColor = V4(m_theme.keyRadiance, m_theme.keySoftness);
    // The depth bias, in world units the same whatever the shadow's range; the fog begins well beyond the board.
    const float fogStart = Length(scene.eye) + 1.2f;
    block.shadowParams = {m_shadowRange, 0.0015f / m_shadowRange, fogStart, 0};
    block.viewport = {static_cast<float>(m_width), static_cast<float>(m_height), 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height)};
    block.environment = {m_theme.diffuseLight, m_theme.reflectedLight, static_cast<float>(m_prefilteredLevels - 1), m_theme.background};
    block.accent = V4(m_theme.accent, 1);
    block.fog = V4(m_theme.fog, 0.15f);
    glNamedBufferSubData(m_frameData.Id(), 0, sizeof(block), &block);
}

// The checkers, then the dice, into the instance buffer.
void Renderer::UploadPieces(const Scene& scene)
{
    std::vector<PieceInstance> instances;
    for (const int kind : {0, 1})
        for (const PieceDraw& piece : scene.pieces)
            if (piece.kind == kind && instances.size() < MaxPieces)
                instances.push_back({piece.model, {static_cast<float>(piece.kind), static_cast<float>(piece.side), static_cast<float>(piece.value), piece.highlight},
                                     {piece.dim, 0, 0, 0}});
    m_checkers = static_cast<int>(std::ranges::count_if(instances, [](const PieceInstance& p) { return p.params.x < 0.5f; }));
    m_dice = static_cast<int>(instances.size()) - m_checkers;
    if (!instances.empty())
        glNamedBufferSubData(m_pieceBuffer.Id(), 0, static_cast<GLsizeiptr>(instances.size() * sizeof(PieceInstance)), instances.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pieceBuffer.Id());
}

void Renderer::SetPass(Vec3 eye, bool shadows)
{
    const PassBlock block{V4(eye, shadows ? 0.0f : 1.0f)};
    glNamedBufferSubData(m_passData.Id(), 0, sizeof(block), &block);
}

void Renderer::BindLighting() const
{
    glBindTextureUnit(0, m_shadowMap.Id());
    glBindSampler(0, m_shadowCompareSampler.Id());
    glBindTextureUnit(1, m_shadowMap.Id());
    glBindSampler(1, m_shadowDepthSampler.Id());
    glBindTextureUnit(2, m_irradiance.Id());
    glBindTextureUnit(3, m_prefiltered.Id());
    glBindTextureUnit(4, m_brdf.Id());
}

void Renderer::Render(const Scene& scene)
{
    UpdateFrameData(scene);
    UploadPieces(scene);
    DrawShadowMap();
    DrawMainPass(scene);
    PostProcess(scene);
}

void Renderer::DrawPieces(const Program& program) const
{
    program.Set("uFirst", 0);
    m_checker.Draw(m_checkers);
    program.Set("uFirst", m_checkers);
    m_die.Draw(m_dice);
}

void Renderer::DrawShadowMap()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer.Id());
    glViewport(0, 0, ShadowSize, ShadowSize);
    const float farDepth = 1;
    glClearNamedFramebufferfv(m_shadowFramebuffer.Id(), GL_DEPTH, 0, &farDepth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 3.0f);

    const Program& program = m_shadowProgram;
    program.Use();
    program.Set("uInstanced", 0);
    program.Set("uModel", Mat4::Identity());
    m_wood.Draw();
    program.Set("uInstanced", 1);
    DrawPieces(program);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::SetMaterials(const Scene& scene) const
{
    const Theme& t = m_theme;
    const Program& b = m_boardProgram;
    b.Set("uLayout", Vec4{board::PointWidth, board::PointLength, board::HalfDepth, board::BarHalf});
    b.Set("uFieldColor", t.field);
    b.Set("uFieldStyle", Vec4{t.fieldRoughness, t.fieldPattern, t.fieldSeed, t.fieldClearcoat});
    b.Set("uPointColor[0]", t.pointA);
    b.Set("uPointColor[1]", t.pointB);
    b.Set("uPointStyle", Vec4{t.pointRoughness, t.lineWidth, t.lineMetallic, 0});
    b.Set("uLineColor", t.line);
    b.Set("uWoodColor", t.wood);
    b.Set("uWoodStyle", Vec4{t.woodRoughness, t.woodClearcoat, t.woodPattern, t.woodSeed});
    b.Set("uFeltColor", t.felt);
    b.Set("uTableColor", t.table);
    b.Set("uTableStyle", Vec4{t.tableRoughness, t.tablePattern, t.tableSeed, 0});
    b.SetArray("uPoints", scene.points);
    b.Set("uTrays", scene.trays);
    b.Set("uHintColor", t.hint);

    const Program& p = m_pieceProgram;
    p.SetArray("uCheckerColor", std::array<Vec4, 2>{V4(t.checkers[0].color, t.checkers[0].roughness), V4(t.checkers[1].color, t.checkers[1].roughness)});
    p.SetArray("uCheckerStyle", std::array<Vec4, 2>{Vec4{t.checkers[0].clearcoat, t.checkers[0].pattern, 0.3f, t.checkers[0].ornament},
                                                   Vec4{t.checkers[1].clearcoat, t.checkers[1].pattern, 0.7f, t.checkers[1].ornament}});
    p.Set("uDieColor", V4(t.die, t.dieRoughness));
    p.Set("uPipColor", V4(t.pip, 0));
    p.Set("uCheckerTop", board::CheckerHeight / board::CheckerRadius);
}

void Renderer::DrawMainPass(const Scene& scene)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer.Id());
    glViewport(0, 0, m_width, m_height);
    const float clear[4] = {m_theme.fog.x, m_theme.fog.y, m_theme.fog.z, 1};
    const float farDepth = 1;
    glClearNamedFramebufferfv(m_sceneFramebuffer.Id(), GL_COLOR, 0, clear);
    glClearNamedFramebufferfv(m_sceneFramebuffer.Id(), GL_DEPTH, 0, &farDepth);

    SetPass(scene.eye, true);
    BindLighting();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    SetMaterials(scene);
    const Program& board = m_boardProgram;
    board.Use();
    board.Set("uKind", 3);
    m_table.Draw();
    board.Set("uKind", 1);
    m_wood.Draw();
    board.Set("uKind", 2);
    m_felt.Draw();
    board.Set("uKind", 0);
    m_field.Draw();

    m_pieceProgram.Use();
    DrawPieces(m_pieceProgram);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    m_backgroundProgram.Use();
    glBindVertexArray(m_emptyVertexArray.Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Renderer::PostProcess(const Scene& scene)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_emptyVertexArray.Id());

    glBindFramebuffer(GL_FRAMEBUFFER, m_resolvedFramebuffer.Id());
    glViewport(0, 0, m_width, m_height);
    m_resolveProgram.Set("uSamples", m_samples);
    m_resolveProgram.Use();
    glBindTextureUnit(0, m_sceneColor.Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Bloom: halve down to a small image, then add the levels back up.
    m_bloomDownProgram.Use();
    for (std::size_t level = 0; level < BloomLevels; ++level) {
        const auto [width, height] = m_bloomSizes[level];
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[level].Id());
        glViewport(0, 0, width, height);
        const int sourceWidth = level == 0 ? m_width : m_bloomSizes[level - 1][0];
        const int sourceHeight = level == 0 ? m_height : m_bloomSizes[level - 1][1];
        glBindTextureUnit(0, level == 0 ? m_resolved.Id() : m_bloom[level - 1].Id());
        m_bloomDownProgram.Set("uSourceTexel", Vec2{1.0f / static_cast<float>(sourceWidth), 1.0f / static_cast<float>(sourceHeight)});
        m_bloomDownProgram.Set("uFirst", level == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    m_bloomUpProgram.Use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int level = BloomLevels - 2; level >= 0; --level) {
        const auto index = static_cast<std::size_t>(level);
        const auto [width, height] = m_bloomSizes[index];
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[index].Id());
        glViewport(0, 0, width, height);
        glBindTextureUnit(0, m_bloom[index + 1].Id());
        m_bloomUpProgram.Set("uSourceTexel", Vec2{1.0f / static_cast<float>(m_bloomSizes[index + 1][0]), 1.0f / static_cast<float>(m_bloomSizes[index + 1][1])});
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    const Program& composite = m_compositeProgram;
    composite.Set("uExposure", m_theme.exposure);
    composite.Set("uBloomStrength", m_theme.bloom);
    composite.Set("uVignette", m_theme.vignette);
    std::array<Vec4, 4> panels{}, styles{};
    const std::size_t count = std::min<std::size_t>(scene.panels.size(), 4);
    for (std::size_t i = 0; i < count; ++i) {
        const FrostedPanel& panel = scene.panels[i];
        panels[i] = {panel.x + panel.width / 2, panel.y + panel.height / 2, panel.width / 2, panel.height / 2};
        styles[i] = {panel.radius, panel.opacity, 0, 0};
    }
    composite.SetArray("uPanels", panels);
    composite.SetArray("uPanelStyle", styles);
    composite.Set("uPanelCount", static_cast<int>(count));
    composite.Use();
    glBindTextureUnit(0, m_resolved.Id());
    glBindTextureUnit(1, m_bloom[0].Id());
    glBindTextureUnit(2, m_bloom[2].Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace render
