/*
 * Copyright (C) 2017 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "PeerConnectionBackend.h"

namespace WebCore {

class LibWebRTCMediaEndpoint;
class RTCRtpReceiver;
class RTCSessionDescription;

class LibWebRTCPeerConnectionBackend final : public PeerConnectionBackend {
public:
    explicit LibWebRTCPeerConnectionBackend(RTCPeerConnection&);

private:
    void doCreateOffer(RTCOfferOptions&&) final;
    void doCreateAnswer(RTCAnswerOptions&&) final;
    void doSetLocalDescription(RTCSessionDescription&) final;
    void doSetRemoteDescription(RTCSessionDescription&) final;
    void doAddIceCandidate(RTCIceCandidate&) final;
    void doStop() final;
    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;
    void setConfiguration(MediaEndpointConfiguration&&) final;
    void getStats(MediaStreamTrack*, PeerConnection::StatsPromise&&) final;
    Ref<RTCRtpReceiver> createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId) final;

    // FIXME: API to implement for real
    RefPtr<RTCSessionDescription> localDescription() const final { return nullptr; }
    RefPtr<RTCSessionDescription> currentLocalDescription() const final { return nullptr; }
    RefPtr<RTCSessionDescription> pendingLocalDescription() const final { return nullptr; }

    RefPtr<RTCSessionDescription> remoteDescription() const final { return nullptr; }
    RefPtr<RTCSessionDescription> currentRemoteDescription() const final { return nullptr; }
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const final { return nullptr; }


    Vector<RefPtr<MediaStream>> getRemoteStreams() const final { return { }; }

    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromise<void>&&) final { }

    bool isNegotiationNeeded() const final { return false; }
    void markAsNeedingNegotiation() final;
    void clearNegotiationNeededState() final { }

    void emulatePlatformEvent(const String&) final { }

    friend LibWebRTCMediaEndpoint;
    RTCPeerConnection& connection() { return m_peerConnection; }

private:
    Ref<LibWebRTCMediaEndpoint> m_endpoint;
    bool m_isLocalDescriptionSet { false };
    bool m_isRemoteDescriptionSet { false };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
