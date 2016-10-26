/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CDMPrivateClearKey.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMSessionClearKey.h"
#include "ContentType.h"
#include "ExceptionCode.h"
#include "MediaPlayer.h"

namespace WebCore {

bool CDMPrivateClearKey::supportsKeySystem(const String& keySystem)
{
    if (!equalLettersIgnoringASCIICase(keySystem, "org.w3c.clearkey"))
        return false;

    // The MediaPlayer must also support the key system:
    return MediaPlayer::supportsKeySystem(keySystem, emptyString());
}

bool CDMPrivateClearKey::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(keySystem, "org.w3c.clearkey"))
        return false;

    // The MediaPlayer must also support the key system:
    return MediaPlayer::supportsKeySystem(keySystem, mimeType);
}

bool CDMPrivateClearKey::supportsMIMEType(const String& mimeType)
{
    return MediaPlayer::supportsKeySystem(m_cdm->keySystem(), mimeType);
}

std::unique_ptr<CDMSession> CDMPrivateClearKey::createSession(CDMSessionClient* client)
{
    return std::make_unique<CDMSessionClearKey>(client);
}

}

#endif
