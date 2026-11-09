#pragma once

#include "Win32.h"

#include <cstddef>
#include <utility>

// The OpenGL 4.6 core functions the game uses, loaded at run time, and the context they run in.
namespace gl {

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLdouble = double;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;
using GLDEBUGPROC = void(APIENTRY*)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

// clang-format off
constexpr GLenum GL_FALSE = 0, GL_TRUE = 1, GL_NONE = 0;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x0100, GL_STENCIL_BUFFER_BIT = 0x0400, GL_COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum GL_TRIANGLES = 0x0004, GL_TRIANGLE_STRIP = 0x0005;
constexpr GLenum GL_LESS = 0x0201, GL_EQUAL = 0x0202, GL_LEQUAL = 0x0203, GL_GREATER = 0x0204, GL_GEQUAL = 0x0206, GL_ALWAYS = 0x0207;
constexpr GLenum GL_ZERO = 0, GL_ONE = 1, GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_FRONT = 0x0404, GL_BACK = 0x0405, GL_CW = 0x0900, GL_CCW = 0x0901;
constexpr GLenum GL_CULL_FACE = 0x0B44, GL_DEPTH_TEST = 0x0B71, GL_BLEND = 0x0BE2, GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_POLYGON_OFFSET_FILL = 0x8037, GL_MULTISAMPLE = 0x809D, GL_SAMPLE_ALPHA_TO_COVERAGE = 0x809E, GL_FRAMEBUFFER_SRGB = 0x8DB9;
constexpr GLenum GL_CLIP_DISTANCE0 = 0x3000, GL_TEXTURE_CUBE_MAP_SEAMLESS = 0x884F;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5, GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_SHORT = 0x1403, GL_INT = 0x1404, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406;
constexpr GLenum GL_RED = 0x1903, GL_RGB = 0x1907, GL_RGBA = 0x1908, GL_BGRA = 0x80E1, GL_DEPTH_COMPONENT = 0x1902;
constexpr GLenum GL_VENDOR = 0x1F00, GL_RENDERER = 0x1F01, GL_VERSION = 0x1F02;
constexpr GLenum GL_MAJOR_VERSION = 0x821B, GL_MINOR_VERSION = 0x821C;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE_CUBE_MAP = 0x8513, GL_TEXTURE_2D_MULTISAMPLE = 0x9100;
constexpr GLenum GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_LINEAR_MIPMAP_NEAREST = 0x2701, GL_LINEAR_MIPMAP_LINEAR = 0x2703;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_TEXTURE_WRAP_R = 0x8072;
constexpr GLenum GL_REPEAT = 0x2901, GL_CLAMP_TO_EDGE = 0x812F, GL_CLAMP_TO_BORDER = 0x812D;
constexpr GLenum GL_TEXTURE_BORDER_COLOR = 0x1004, GL_TEXTURE_MAX_LEVEL = 0x813D;
constexpr GLenum GL_TEXTURE_COMPARE_MODE = 0x884C, GL_TEXTURE_COMPARE_FUNC = 0x884D, GL_COMPARE_REF_TO_TEXTURE = 0x884E;
constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY = 0x84FE, GL_MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;
constexpr GLenum GL_R8 = 0x8229, GL_RG16F = 0x822F, GL_R16F = 0x822D, GL_RGBA8 = 0x8058, GL_RGBA16F = 0x881A, GL_R11F_G11F_B10F = 0x8C3A;
constexpr GLenum GL_R8UI = 0x8232, GL_RED_INTEGER = 0x8D94;
constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6, GL_DEPTH_COMPONENT32F = 0x8CAC;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893, GL_UNIFORM_BUFFER = 0x8A11, GL_SHADER_STORAGE_BUFFER = 0x90D2;
constexpr GLenum GL_STREAM_DRAW = 0x88E0, GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLbitfield GL_DYNAMIC_STORAGE_BIT = 0x0100;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31, GL_COMPUTE_SHADER = 0x91B9;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82, GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40, GL_READ_FRAMEBUFFER = 0x8CA8, GL_DRAW_FRAMEBUFFER = 0x8CA9, GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0, GL_DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum GL_COLOR = 0x1800, GL_DEPTH = 0x1801;
constexpr GLenum GL_MAX_SAMPLES = 0x8D57, GL_MAX_COLOR_TEXTURE_SAMPLES = 0x910E, GL_MAX_DEPTH_TEXTURE_SAMPLES = 0x910F;
constexpr GLenum GL_READ_ONLY = 0x88B8, GL_WRITE_ONLY = 0x88B9;
constexpr GLbitfield GL_TEXTURE_FETCH_BARRIER_BIT = 0x0008, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x0020, GL_ALL_BARRIER_BITS = 0xFFFFFFFF;
constexpr GLenum GL_DEBUG_OUTPUT = 0x92E0, GL_DEBUG_OUTPUT_SYNCHRONOUS = 0x8242;
constexpr GLenum GL_DEBUG_SEVERITY_HIGH = 0x9146, GL_DEBUG_SEVERITY_MEDIUM = 0x9147, GL_DEBUG_SEVERITY_LOW = 0x9148, GL_DEBUG_SEVERITY_NOTIFICATION = 0x826B;
// clang-format on

// Return type, name, parameters.
#define NARDY_GL_FUNCTIONS(X)                                                                                                          \
    X(void, glGetIntegerv, GLenum name, GLint* data)                                                                                   \
    X(void, glGetFloatv, GLenum name, GLfloat* data)                                                                                   \
    X(void, glEnable, GLenum capability)                                                                                               \
    X(void, glDisable, GLenum capability)                                                                                              \
    X(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height)                                                               \
    X(void, glClear, GLbitfield mask)                                                                                                  \
    X(void, glClearColor, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)                                                     \
    X(void, glDepthFunc, GLenum function)                                                                                              \
    X(void, glDepthMask, GLboolean flag)                                                                                               \
    X(void, glCullFace, GLenum mode)                                                                                                   \
    X(void, glBlendFunc, GLenum source, GLenum destination)                                                                            \
    X(void, glBlendFuncSeparate, GLenum sourceRgb, GLenum destinationRgb, GLenum sourceAlpha, GLenum destinationAlpha)                 \
    X(void, glPolygonOffset, GLfloat factor, GLfloat units)                                                                            \
    X(void, glPixelStorei, GLenum name, GLint value)                                                                                   \
    X(void, glDrawArrays, GLenum mode, GLint first, GLsizei count)                                                                     \
    X(void, glDrawElements, GLenum mode, GLsizei count, GLenum type, const void* indices)                                              \
    X(void, glDrawElementsInstanced, GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instances)                  \
    X(void, glCreateBuffers, GLsizei count, GLuint* buffers)                                                                           \
    X(void, glDeleteBuffers, GLsizei count, const GLuint* buffers)                                                                     \
    X(void, glNamedBufferStorage, GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags)                                  \
    X(void, glNamedBufferData, GLuint buffer, GLsizeiptr size, const void* data, GLenum usage)                                         \
    X(void, glNamedBufferSubData, GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data)                                   \
    X(void, glBindBufferBase, GLenum target, GLuint index, GLuint buffer)                                                              \
    X(void, glCreateVertexArrays, GLsizei count, GLuint* arrays)                                                                       \
    X(void, glDeleteVertexArrays, GLsizei count, const GLuint* arrays)                                                                 \
    X(void, glBindVertexArray, GLuint array)                                                                                           \
    X(void, glVertexArrayVertexBuffer, GLuint array, GLuint binding, GLuint buffer, GLintptr offset, GLsizei stride)                   \
    X(void, glVertexArrayElementBuffer, GLuint array, GLuint buffer)                                                                   \
    X(void, glEnableVertexArrayAttrib, GLuint array, GLuint index)                                                                     \
    X(void, glVertexArrayAttribFormat, GLuint array, GLuint index, GLint size, GLenum type, GLboolean normalized, GLuint offset)       \
    X(void, glVertexArrayAttribBinding, GLuint array, GLuint index, GLuint binding)                                                    \
    X(void, glCreateTextures, GLenum target, GLsizei count, GLuint* textures)                                                          \
    X(void, glDeleteTextures, GLsizei count, const GLuint* textures)                                                                   \
    X(void, glTextureStorage2D, GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height)                          \
    X(void, glTextureStorage2DMultisample, GLuint texture, GLsizei samples, GLenum format, GLsizei width, GLsizei height,               \
      GLboolean fixedLocations)                                                                                                        \
    X(void, glTextureSubImage2D, GLuint texture, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,          \
      GLenum type, const void* pixels)                                                                                                 \
    X(void, glTextureParameteri, GLuint texture, GLenum name, GLint value)                                                             \
    X(void, glTextureParameterf, GLuint texture, GLenum name, GLfloat value)                                                           \
    X(void, glGenerateTextureMipmap, GLuint texture)                                                                                   \
    X(void, glBindTextureUnit, GLuint unit, GLuint texture)                                                                            \
    X(void, glBindImageTexture, GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) \
    X(void, glCreateSamplers, GLsizei count, GLuint* samplers)                                                                         \
    X(void, glDeleteSamplers, GLsizei count, const GLuint* samplers)                                                                   \
    X(void, glSamplerParameteri, GLuint sampler, GLenum name, GLint value)                                                             \
    X(void, glBindSampler, GLuint unit, GLuint sampler)                                                                                \
    X(void, glCreateFramebuffers, GLsizei count, GLuint* framebuffers)                                                                 \
    X(void, glDeleteFramebuffers, GLsizei count, const GLuint* framebuffers)                                                           \
    X(void, glNamedFramebufferTexture, GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)                             \
    X(void, glNamedFramebufferDrawBuffer, GLuint framebuffer, GLenum buffer)                                                           \
    X(GLenum, glCheckNamedFramebufferStatus, GLuint framebuffer, GLenum target)                                                        \
    X(void, glBindFramebuffer, GLenum target, GLuint framebuffer)                                                                      \
    X(void, glClearNamedFramebufferfv, GLuint framebuffer, GLenum buffer, GLint drawBuffer, const GLfloat* value)                      \
    X(GLuint, glCreateShader, GLenum type)                                                                                             \
    X(void, glShaderSource, GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths)                           \
    X(void, glCompileShader, GLuint shader)                                                                                            \
    X(void, glGetShaderiv, GLuint shader, GLenum name, GLint* value)                                                                   \
    X(void, glGetShaderInfoLog, GLuint shader, GLsizei capacity, GLsizei* length, GLchar* log)                                         \
    X(void, glDeleteShader, GLuint shader)                                                                                             \
    X(GLuint, glCreateProgram)                                                                                                         \
    X(void, glAttachShader, GLuint program, GLuint shader)                                                                             \
    X(void, glLinkProgram, GLuint program)                                                                                             \
    X(void, glGetProgramiv, GLuint program, GLenum name, GLint* value)                                                                 \
    X(void, glGetProgramInfoLog, GLuint program, GLsizei capacity, GLsizei* length, GLchar* log)                                       \
    X(void, glDeleteProgram, GLuint program)                                                                                           \
    X(void, glUseProgram, GLuint program)                                                                                              \
    X(GLint, glGetUniformLocation, GLuint program, const GLchar* name)                                                                 \
    X(void, glProgramUniform1i, GLuint program, GLint location, GLint value)                                                           \
    X(void, glProgramUniform1f, GLuint program, GLint location, GLfloat value)                                                         \
    X(void, glProgramUniform2f, GLuint program, GLint location, GLfloat x, GLfloat y)                                                  \
    X(void, glProgramUniform3f, GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z)                                       \
    X(void, glProgramUniform4f, GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)                            \
    X(void, glProgramUniform4fv, GLuint program, GLint location, GLsizei count, const GLfloat* values)                                 \
    X(void, glProgramUniformMatrix4fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* values)      \
    X(void, glDispatchCompute, GLuint x, GLuint y, GLuint z)                                                                           \
    X(void, glMemoryBarrier, GLbitfield barriers)                                                                                      \
    X(void, glDebugMessageCallback, GLDEBUGPROC callback, const void* userParam)

#define NARDY_GL_DECLARE(type, name, ...) extern type(APIENTRY* name)(__VA_ARGS__);
NARDY_GL_FUNCTIONS(NARDY_GL_DECLARE)
#undef NARDY_GL_DECLARE

// Owns an OpenGL object name and deletes it with the given function.
template <typename Deleter>
class Object {
public:
    Object() = default;
    explicit Object(GLuint id) : m_id(id) {}
    ~Object() { Reset(); }
    Object(Object&& other) noexcept : m_id(std::exchange(other.m_id, 0)) {}
    Object& operator=(Object&& other) noexcept
    {
        if (this != &other) {
            Reset();
            m_id = std::exchange(other.m_id, 0);
        }
        return *this;
    }
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    GLuint Id() const { return m_id; }
    explicit operator bool() const { return m_id != 0; }
    void Reset()
    {
        if (m_id != 0)
            Deleter{}(m_id);
        m_id = 0;
    }

private:
    GLuint m_id = 0;
};

struct BufferDeleter { void operator()(GLuint id) const { glDeleteBuffers(1, &id); } };
struct VertexArrayDeleter { void operator()(GLuint id) const { glDeleteVertexArrays(1, &id); } };
struct TextureDeleter { void operator()(GLuint id) const { glDeleteTextures(1, &id); } };
struct SamplerDeleter { void operator()(GLuint id) const { glDeleteSamplers(1, &id); } };
struct FramebufferDeleter { void operator()(GLuint id) const { glDeleteFramebuffers(1, &id); } };
struct ProgramDeleter { void operator()(GLuint id) const { glDeleteProgram(id); } };

using Buffer = Object<BufferDeleter>;
using VertexArray = Object<VertexArrayDeleter>;
using Texture = Object<TextureDeleter>;
using Sampler = Object<SamplerDeleter>;
using Framebuffer = Object<FramebufferDeleter>;
using ProgramObject = Object<ProgramDeleter>;

Buffer CreateBuffer(const void* data, std::size_t size, GLbitfield flags = 0);
Texture CreateTexture(GLenum target);
Framebuffer CreateFramebuffer();

// A window's OpenGL 4.6 (or at least 4.5) core context, current on the calling thread.
class Context {
public:
    explicit Context(HWND window);
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void SwapBuffers() const { ::SwapBuffers(m_dc); }
    int Version() const { return m_version; } // 46 for 4.6

private:
    HWND m_window = nullptr;
    HDC m_dc = nullptr;
    HGLRC m_context = nullptr;
    int m_version = 0;
};

} // namespace gl
