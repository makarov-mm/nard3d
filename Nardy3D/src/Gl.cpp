#include "Gl.h"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>

namespace gl {

#define NARDY_GL_DEFINE(type, name, ...) type(APIENTRY* name)(__VA_ARGS__) = nullptr;
NARDY_GL_FUNCTIONS(NARDY_GL_DEFINE)
#undef NARDY_GL_DEFINE

namespace {

// WGL_ARB_pixel_format, WGL_ARB_create_context, WGL_ARB_create_context_profile
constexpr int WGL_DRAW_TO_WINDOW_ARB = 0x2001, WGL_ACCELERATION_ARB = 0x2003, WGL_SUPPORT_OPENGL_ARB = 0x2010;
constexpr int WGL_DOUBLE_BUFFER_ARB = 0x2011, WGL_PIXEL_TYPE_ARB = 0x2013, WGL_COLOR_BITS_ARB = 0x2014, WGL_ALPHA_BITS_ARB = 0x201B;
constexpr int WGL_DEPTH_BITS_ARB = 0x2022, WGL_STENCIL_BITS_ARB = 0x2023, WGL_FULL_ACCELERATION_ARB = 0x2027, WGL_TYPE_RGBA_ARB = 0x202B;
constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091, WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126, WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;
[[maybe_unused]] constexpr int WGL_CONTEXT_FLAGS_ARB = 0x2094, WGL_CONTEXT_DEBUG_BIT_ARB = 0x0001; // debug builds

using ChoosePixelFormatArb = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
using CreateContextAttribsArb = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using SwapIntervalExt = BOOL(WINAPI*)(int);

template <typename Function>
Function Cast(PROC proc)
{
    return reinterpret_cast<Function>(reinterpret_cast<void (*)()>(proc));
}

// Extension functions and the ones newer than OpenGL 1.1 come from the driver; the rest from opengl32.dll.
PROC GetProc(const char* name)
{
    PROC proc = wglGetProcAddress(name);
    const auto value = reinterpret_cast<std::intptr_t>(proc);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
        static const HMODULE library = LoadLibraryW(L"opengl32.dll");
        proc = GetProcAddress(library, name);
    }
    return proc;
}

PIXELFORMATDESCRIPTOR BasicPixelFormat()
{
    PIXELFORMATDESCRIPTOR descriptor{};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cAlphaBits = 8;
    descriptor.cDepthBits = 24;
    descriptor.cStencilBits = 8;
    descriptor.iLayerType = PFD_MAIN_PLANE;
    return descriptor;
}

struct Extensions {
    ChoosePixelFormatArb choosePixelFormat = nullptr;
    CreateContextAttribsArb createContextAttribs = nullptr;
};

// The functions that create a modern context are found through a context: a throwaway one on a hidden window.
Extensions LoadExtensions()
{
    Extensions extensions;
    HWND window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return extensions;
    HDC dc = GetDC(window);
    const PIXELFORMATDESCRIPTOR descriptor = BasicPixelFormat();
    const int format = ChoosePixelFormat(dc, &descriptor);
    if (format != 0 && SetPixelFormat(dc, format, &descriptor)) {
        if (HGLRC context = wglCreateContext(dc)) {
            if (wglMakeCurrent(dc, context)) {
                extensions.choosePixelFormat = Cast<ChoosePixelFormatArb>(wglGetProcAddress("wglChoosePixelFormatARB"));
                extensions.createContextAttribs = Cast<CreateContextAttribsArb>(wglGetProcAddress("wglCreateContextAttribsARB"));
                wglMakeCurrent(nullptr, nullptr);
            }
            wglDeleteContext(context);
        }
    }
    ReleaseDC(window, dc);
    DestroyWindow(window);
    return extensions;
}

void LoadFunctions()
{
    std::vector<std::string> missing;
#define NARDY_GL_LOAD(type, name, ...)                    \
    name = Cast<decltype(name)>(GetProc(#name));           \
    if (name == nullptr)                                   \
        missing.emplace_back(#name);
    NARDY_GL_FUNCTIONS(NARDY_GL_LOAD)
#undef NARDY_GL_LOAD
    if (!missing.empty())
        throw std::runtime_error(std::format("The graphics driver lacks {} OpenGL functions, such as {}.", missing.size(), missing.front()));
}

#ifdef _DEBUG
void APIENTRY DebugMessage(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM) {
        OutputDebugStringA("OpenGL: ");
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
}
#endif

} // namespace

Buffer CreateBuffer(const void* data, std::size_t size, GLbitfield flags)
{
    GLuint id = 0;
    glCreateBuffers(1, &id);
    glNamedBufferStorage(id, static_cast<GLsizeiptr>(size), data, flags);
    return Buffer(id);
}

Texture CreateTexture(GLenum target)
{
    GLuint id = 0;
    glCreateTextures(target, 1, &id);
    return Texture(id);
}

Framebuffer CreateFramebuffer()
{
    GLuint id = 0;
    glCreateFramebuffers(1, &id);
    return Framebuffer(id);
}

Context::Context(HWND window) : m_window(window)
{
    const Extensions extensions = LoadExtensions();
    if (!extensions.choosePixelFormat || !extensions.createContextAttribs)
        throw std::runtime_error("The graphics driver does not support modern OpenGL contexts (WGL_ARB_create_context).");

    m_dc = GetDC(window);
    const int attributes[] = {WGL_DRAW_TO_WINDOW_ARB, 1, WGL_SUPPORT_OPENGL_ARB, 1, WGL_DOUBLE_BUFFER_ARB, 1, WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                              WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB, WGL_COLOR_BITS_ARB, 24, WGL_ALPHA_BITS_ARB, 8, WGL_DEPTH_BITS_ARB, 24,
                              WGL_STENCIL_BITS_ARB, 8, 0};
    int format = 0;
    UINT count = 0;
    if (!extensions.choosePixelFormat(m_dc, attributes, nullptr, 1, &format, &count) || count == 0)
        format = 0;
    PIXELFORMATDESCRIPTOR descriptor = BasicPixelFormat();
    if (format == 0)
        format = ChoosePixelFormat(m_dc, &descriptor);
    if (format == 0 || !DescribePixelFormat(m_dc, format, sizeof(descriptor), &descriptor) || !SetPixelFormat(m_dc, format, &descriptor))
        throw std::runtime_error("The window has no OpenGL pixel format.");

    // OpenGL 4.6 core, or 4.5 where the driver stops there.
    for (int minor : {6, 5}) {
        const int contextAttributes[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 4, WGL_CONTEXT_MINOR_VERSION_ARB, minor, WGL_CONTEXT_PROFILE_MASK_ARB,
                                         WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
                                         WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                                         0};
        m_context = extensions.createContextAttribs(m_dc, nullptr, contextAttributes);
        if (m_context)
            break;
    }
    if (!m_context || !wglMakeCurrent(m_dc, m_context)) {
        if (m_context)
            wglDeleteContext(m_context);
        ReleaseDC(window, m_dc);
        throw std::runtime_error("OpenGL 4.5 or newer is not available. Please update the graphics driver.");
    }

    try {
        LoadFunctions();
    }
    catch (...) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_context);
        ReleaseDC(window, m_dc);
        throw;
    }

    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    m_version = major * 10 + minor;

    if (auto swapInterval = Cast<SwapIntervalExt>(wglGetProcAddress("wglSwapIntervalEXT")))
        swapInterval(1);

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(DebugMessage, nullptr);
#endif
}

Context::~Context()
{
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(m_context);
    ReleaseDC(m_window, m_dc);
}

} // namespace gl
