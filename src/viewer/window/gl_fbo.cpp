#include "gl_fbo.h"
#include <SDL2/SDL.h>
#include <stdexcept>

PFNGLGENFRAMEBUFFERSPROC        glGenFramebuffers_        = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC     glDeleteFramebuffers_     = nullptr;
PFNGLBINDFRAMEBUFFERPROC        glBindFramebuffer_        = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC   glFramebufferTexture2D_   = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ = nullptr;
PFNGLGENRENDERBUFFERSPROC       glGenRenderbuffers_       = nullptr;
PFNGLDELETERENDERBUFFERSPROC    glDeleteRenderbuffers_    = nullptr;
PFNGLBINDRENDERBUFFERPROC       glBindRenderbuffer_       = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC    glRenderbufferStorage_    = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ = nullptr;

namespace {
template <typename T>
T loadProc(const char* name) {
    return reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
}
}

void loadFramebufferFunctions() {
    glGenFramebuffers_        = loadProc<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    glDeleteFramebuffers_     = loadProc<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
    glBindFramebuffer_        = loadProc<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    glFramebufferTexture2D_   = loadProc<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    glCheckFramebufferStatus_ = loadProc<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    glGenRenderbuffers_       = loadProc<PFNGLGENRENDERBUFFERSPROC>("glGenRenderbuffers");
    glDeleteRenderbuffers_    = loadProc<PFNGLDELETERENDERBUFFERSPROC>("glDeleteRenderbuffers");
    glBindRenderbuffer_       = loadProc<PFNGLBINDRENDERBUFFERPROC>("glBindRenderbuffer");
    glRenderbufferStorage_    = loadProc<PFNGLRENDERBUFFERSTORAGEPROC>("glRenderbufferStorage");
    glFramebufferRenderbuffer_ = loadProc<PFNGLFRAMEBUFFERRENDERBUFFERPROC>("glFramebufferRenderbuffer");

    if (!glGenFramebuffers_ || !glDeleteFramebuffers_ || !glBindFramebuffer_ ||
        !glFramebufferTexture2D_ || !glCheckFramebufferStatus_ ||
        !glGenRenderbuffers_ || !glDeleteRenderbuffers_ || !glBindRenderbuffer_ ||
        !glRenderbufferStorage_ || !glFramebufferRenderbuffer_)
        throw std::runtime_error("OpenGL framebuffer object functions are not available");
}
