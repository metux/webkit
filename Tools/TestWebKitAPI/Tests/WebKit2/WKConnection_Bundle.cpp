/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "InjectedBundleTest.h"
#include "PlatformUtilities.h"
#include <WebKit2/WKRetainPtr.h>

namespace TestWebKitAPI {

/* WKConnectionClient */
static void connectionDidReceiveMessage(WKConnectionRef connection, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    // Post a simple message to the back to the application layer.
    WKConnectionPostMessage(connection, Util::toWK("PongMessageName").get(), Util::toWK("PongMessageBody").get());
}

class WKConnectionTest : public InjectedBundleTest {
public:
    WKConnectionTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    virtual void initialize(WKBundleRef bundle, WKTypeRef)
    {
        WKConnectionClientV0 connectionClient;
        memset(&connectionClient, 0, sizeof(connectionClient));

        connectionClient.base.version = WKConnectionClientCurrentVersion;
        connectionClient.didReceiveMessage = connectionDidReceiveMessage;

        WKConnectionSetConnectionClient(WKBundleGetApplicationConnection(bundle), &connectionClient.base);
    }
};

static InjectedBundleTest::Register<WKConnectionTest> registrar("WKConnectionTest");

} // namespace TestWebKitAPI
