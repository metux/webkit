/*
 * Copyright (C) 2015,2016 Igalia S.L
 * Copyright (C) 2015 Metrological
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)

#include "RealtimeMediaSourceOwr.h"

namespace WebCore {

class RealtimeAudioSourceOwr : public RealtimeMediaSourceOwr {
public:
RealtimeAudioSourceOwr(OwrMediaSource* mediaSource, const String& id, RealtimeMediaSource::Type type, const String& name)
    : RealtimeMediaSourceOwr(mediaSource, id, type, name)
    {
    }

RealtimeAudioSourceOwr(const String& id, RealtimeMediaSource::Type type, const String& name)
    : RealtimeMediaSourceOwr(id, type, name)
    {
    }

    virtual ~RealtimeAudioSourceOwr() { }

    bool applySize(const IntSize&) final { return false; }

protected:
    void initializeSettings() final {
        if (m_currentSettings.deviceId().isEmpty())
            m_currentSettings.setSupportedConstraits(supportedConstraints());

        m_currentSettings.setDeviceId(id());
    }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)
