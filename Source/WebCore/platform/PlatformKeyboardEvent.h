/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "KeypressCommand.h"
#include "PlatformEvent.h"
#include <wtf/WindowsExtras.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSEvent;
#endif

#if PLATFORM(IOS)
OBJC_CLASS WebEvent;
#endif

#if PLATFORM(GTK)
typedef struct _GdkEventKey GdkEventKey;
#include "CompositionResults.h"
#endif

#if PLATFORM(EFL)
typedef struct _Evas_Event_Key_Down Evas_Event_Key_Down;
typedef struct _Evas_Event_Key_Up Evas_Event_Key_Up;
#endif

namespace WebCore {

    class PlatformKeyboardEvent : public PlatformEvent {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        PlatformKeyboardEvent()
            : PlatformEvent(PlatformEvent::KeyDown)
            , m_windowsVirtualKeyCode(0)
#if USE(APPKIT) || PLATFORM(GTK)
            , m_handledByInputMethod(false)
#endif
            , m_autoRepeat(false)
            , m_isKeypad(false)
            , m_isSystemKey(false)
#if PLATFORM(GTK)
            , m_gdkEventKey(0)
#endif
        {
        }

        PlatformKeyboardEvent(Type type, const String& text, const String& unmodifiedText,
#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
        const String& key,
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
        const String& code,
#endif
        const String& keyIdentifier, int windowsVirtualKeyCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier> modifiers, double timestamp)
            : PlatformEvent(type, modifiers, timestamp)
            , m_text(text)
            , m_unmodifiedText(unmodifiedText)
#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
            , m_key(key)
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
            , m_code(code)
#endif
            , m_keyIdentifier(keyIdentifier)
            , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
#if USE(APPKIT) || PLATFORM(GTK)
            , m_handledByInputMethod(false)
#endif
            , m_autoRepeat(isAutoRepeat)
            , m_isKeypad(isKeypad)
            , m_isSystemKey(isSystemKey)
        {
        }

        WEBCORE_EXPORT void disambiguateKeyDownEvent(Type, bool backwardCompatibilityMode = false); // Only used on platforms that need it, i.e. those that generate KeyDown events.

        // Text as as generated by processing a virtual key code with a keyboard layout
        // (in most cases, just a character code, but the layout can emit several
        // characters in a single keypress event on some platforms).
        // This may bear no resemblance to the ultimately inserted text if an input method
        // processes the input.
        // Will be null for KeyUp and RawKeyDown events.
        String text() const { return m_text; }

        // Text that would have been generated by the keyboard if no modifiers were pressed
        // (except for Shift); useful for shortcut (accelerator) key handling.
        // Otherwise, same as text().
        String unmodifiedText() const { return m_unmodifiedText; }

        String keyIdentifier() const { return m_keyIdentifier; }

#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
        const String& key() const { return m_key; }
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
        const String& code() const { return m_code; }
#endif

        // Most compatible Windows virtual key code associated with the event.
        // Zero for Char events.
        int windowsVirtualKeyCode() const { return m_windowsVirtualKeyCode; }
        void setWindowsVirtualKeyCode(int code) { m_windowsVirtualKeyCode = code; }

#if USE(APPKIT) || PLATFORM(GTK)
        bool handledByInputMethod() const { return m_handledByInputMethod; }
#endif
#if USE(APPKIT)
        const Vector<KeypressCommand>& commands() const { return m_commands; }
#elif PLATFORM(GTK)
        const Vector<String>& commands() const { return m_commands; }
#endif

        bool isAutoRepeat() const { return m_autoRepeat; }
        bool isKeypad() const { return m_isKeypad; }
        bool isSystemKey() const { return m_isSystemKey; }

        static bool currentCapsLockState();
        static void getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey);

#if PLATFORM(COCOA)
#if !PLATFORM(IOS)
        NSEvent* macEvent() const { return m_macEvent.get(); }
#else
        WebEvent *event() const { return m_Event.get(); }
#endif
#endif

#if PLATFORM(WIN)
        PlatformKeyboardEvent(HWND, WPARAM, LPARAM, Type, bool);
#endif

#if PLATFORM(GTK)
        PlatformKeyboardEvent(GdkEventKey*, const CompositionResults&);
        GdkEventKey* gdkEventKey() const { return m_gdkEventKey; }
        const CompositionResults& compositionResults() const { return m_compositionResults; }

        // Used by WebKit2
        static String keyIdentifierForGdkKeyCode(unsigned);
        static int windowsKeyCodeForGdkKeyCode(unsigned);
        static String singleCharacterString(unsigned);
#endif

#if PLATFORM(EFL)
        explicit PlatformKeyboardEvent(const Evas_Event_Key_Down*);
        explicit PlatformKeyboardEvent(const Evas_Event_Key_Up*);
#endif

    protected:
        String m_text;
        String m_unmodifiedText;
#if ENABLE(KEYBOARD_KEY_ATTRIBUTE)
        String m_key;
#endif
#if ENABLE(KEYBOARD_CODE_ATTRIBUTE)
        String m_code;
#endif
        String m_keyIdentifier;
        int m_windowsVirtualKeyCode;
#if USE(APPKIT) || PLATFORM(GTK)
        bool m_handledByInputMethod;
#endif
#if USE(APPKIT)
        Vector<KeypressCommand> m_commands;
#elif PLATFORM(GTK)
        Vector<String> m_commands;
#endif
        bool m_autoRepeat;
        bool m_isKeypad;
        bool m_isSystemKey;

#if PLATFORM(COCOA)
#if !PLATFORM(IOS)
        RetainPtr<NSEvent> m_macEvent;
#else
        RetainPtr<WebEvent> m_Event;
#endif
#endif
#if PLATFORM(GTK)
        GdkEventKey* m_gdkEventKey;
        CompositionResults m_compositionResults;
#endif
    };
    
} // namespace WebCore
