/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformDisplay_h
#define PlatformDisplay_h

#include <wtf/Noncopyable.h>
#include <wtf/TypeCasts.h>

#if USE(EGL)
typedef void *EGLDisplay;
#endif

namespace WebCore {

class GLContext;

class PlatformDisplay {
    WTF_MAKE_NONCOPYABLE(PlatformDisplay); WTF_MAKE_FAST_ALLOCATED;
public:
    static PlatformDisplay& sharedDisplay();
    static PlatformDisplay& sharedDisplayForCompositing();
    virtual ~PlatformDisplay();

    enum class Type {
#if PLATFORM(X11)
        X11,
#endif
#if PLATFORM(WAYLAND)
        Wayland,
#endif
#if PLATFORM(WIN)
        Windows,
#endif
    };

    virtual Type type() const = 0;

#if !PLATFORM(EFL) && (USE(EGL) || USE(GLX))
    // FIXME: This should not have any platform ifdef, but EFL has its own EGLContext class
    // instead of using the GLContext common API.
    GLContext* sharingGLContext();
#endif

#if USE(EGL)
    EGLDisplay eglDisplay() const;
    bool eglCheckVersion(int major, int minor) const;
#endif

protected:
    PlatformDisplay();

    static void setSharedDisplayForCompositing(PlatformDisplay&);

#if USE(EGL)
    virtual void initializeEGLDisplay();

    EGLDisplay m_eglDisplay;
#endif

#if USE(EGL) || USE(GLX)
    std::unique_ptr<GLContext> m_sharingGLContext;
#endif

private:
    static std::unique_ptr<PlatformDisplay> createPlatformDisplay();

#if USE(EGL)
    void terminateEGLDisplay();

    bool m_eglDisplayInitialized { false };
    int m_eglMajorVersion { 0 };
    int m_eglMinorVersion { 0 };
#endif
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(ToClassName, DisplayType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::PlatformDisplay& display) { return display.type() == WebCore::PlatformDisplay::Type::DisplayType; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // PltformDisplay_h
