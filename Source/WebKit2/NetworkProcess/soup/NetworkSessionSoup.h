/*
 * Copyright (C) 2016 Igalia S.L.
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

#pragma once

#include "NetworkSession.h"
#include <wtf/HashSet.h>

typedef struct _SoupSession SoupSession;

namespace WebKit {

class NetworkDataTaskSoup;

class NetworkSessionSoup final : public NetworkSession {
public:
    static Ref<NetworkSession> create(WebCore::SessionID sessionID)
    {
        return adoptRef(*new NetworkSessionSoup(sessionID));
    }
    ~NetworkSessionSoup();

    SoupSession* soupSession() const;

    void registerNetworkDataTask(NetworkDataTaskSoup& task) { m_dataTaskSet.add(&task); }
    void unregisterNetworkDataTask(NetworkDataTaskSoup& task) { m_dataTaskSet.remove(&task); }

private:
    NetworkSessionSoup(WebCore::SessionID);

    void invalidateAndCancel() override;

    HashSet<NetworkDataTaskSoup*> m_dataTaskSet;
};

} // namespace WebKit
