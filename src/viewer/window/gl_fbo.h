#ifndef PCLITE_GL_FBO_H
#define PCLITE_GL_FBO_H

// The system GL headers only declare GL 1.1 entry points; framebuffer
// objects must be resolved at runtime via SDL_GL_GetProcAddress. This loads
// just the handful of FBO functions Viewport needs to render into a texture.

#include <GL/gl.h>
#include <GL/glext.h>

extern PFNGLGENFRAMEBUFFERSPROC        glGenFramebuffers_;
extern PFNGLDELETEFRAMEBUFFERSPROC     glDeleteFramebuffers_;
extern PFNGLBINDFRAMEBUFFERPROC        glBindFramebuffer_;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC   glFramebufferTexture2D_;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_;
extern PFNGLGENRENDERBUFFERSPROC       glGenRenderbuffers_;
extern PFNGLDELETERENDERBUFFERSPROC    glDeleteRenderbuffers_;
extern PFNGLBINDRENDERBUFFERPROC       glBindRenderbuffer_;
extern PFNGLRENDERBUFFERSTORAGEPROC    glRenderbufferStorage_;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_;

// Resolves the pointers above; throws std::runtime_error if any are missing.
void loadFramebufferFunctions();

#endif // PCLITE_GL_FBO_H
