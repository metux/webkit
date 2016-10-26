/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkStorageSession.h"

#if USE(SOUP)

#include "ResourceHandle.h"
#include "SoupNetworkSession.h"
#include <libsoup/soup.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(LIBSECRET)
#include "GRefPtrGtk.h"
#include <glib/gi18n-lib.h>
#define SECRET_WITH_UNSTABLE 1
#define SECRET_API_SUBJECT_TO_CHANGE 1
#include <libsecret/secret.h>
#endif

namespace WebCore {

NetworkStorageSession::NetworkStorageSession(SessionID sessionID, std::unique_ptr<SoupNetworkSession> session)
    : m_sessionID(sessionID)
    , m_session(WTFMove(session))
{
}

NetworkStorageSession::~NetworkStorageSession()
{
#if USE(LIBSECRET)
    g_cancellable_cancel(m_persisentStorageCancellable.get());
#endif
}

static std::unique_ptr<NetworkStorageSession>& defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

NetworkStorageSession& NetworkStorageSession::defaultStorageSession()
{
    if (!defaultSession())
        defaultSession() = std::make_unique<NetworkStorageSession>(SessionID::defaultSessionID(), nullptr);
    return *defaultSession();
}

void NetworkStorageSession::ensurePrivateBrowsingSession(SessionID sessionID, const String&)
{
    auto session = std::make_unique<NetworkStorageSession>(sessionID, SoupNetworkSession::createPrivateBrowsingSession());
    ASSERT(sessionID != SessionID::defaultSessionID());
    ASSERT(!globalSessionMap().contains(sessionID));
    globalSessionMap().add(sessionID, WTFMove(session));
}

void NetworkStorageSession::switchToNewTestingSession()
{
    defaultSession() = std::make_unique<NetworkStorageSession>(SessionID::defaultSessionID(), SoupNetworkSession::createTestingSession());
}

SoupNetworkSession& NetworkStorageSession::soupNetworkSession() const
{
    return m_session ? *m_session : SoupNetworkSession::defaultSession();
}

#if USE(LIBSECRET)
static const char* schemeFromProtectionSpaceServerType(ProtectionSpaceServerType serverType)
{
    switch (serverType) {
    case ProtectionSpaceServerHTTP:
    case ProtectionSpaceProxyHTTP:
        return SOUP_URI_SCHEME_HTTP;
    case ProtectionSpaceServerHTTPS:
    case ProtectionSpaceProxyHTTPS:
        return SOUP_URI_SCHEME_HTTPS;
    case ProtectionSpaceServerFTP:
    case ProtectionSpaceProxyFTP:
        return SOUP_URI_SCHEME_FTP;
    case ProtectionSpaceServerFTPS:
    case ProtectionSpaceProxySOCKS:
        break;
    }

    ASSERT_NOT_REACHED();
    return SOUP_URI_SCHEME_HTTP;
}

static const char* authTypeFromProtectionSpaceAuthenticationScheme(ProtectionSpaceAuthenticationScheme scheme)
{
    switch (scheme) {
    case ProtectionSpaceAuthenticationSchemeDefault:
    case ProtectionSpaceAuthenticationSchemeHTTPBasic:
        return "Basic";
    case ProtectionSpaceAuthenticationSchemeHTTPDigest:
        return "Digest";
    case ProtectionSpaceAuthenticationSchemeNTLM:
        return "NTLM";
    case ProtectionSpaceAuthenticationSchemeNegotiate:
        return "Negotiate";
    case ProtectionSpaceAuthenticationSchemeHTMLForm:
    case ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
    case ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        ASSERT_NOT_REACHED();
        break;
    case ProtectionSpaceAuthenticationSchemeUnknown:
        return "unknown";
    }

    ASSERT_NOT_REACHED();
    return "unknown";
}
#endif // USE(LIBSECRET)

void NetworkStorageSession::getCredentialFromPersistentStorage(const ProtectionSpace& protectionSpace, Function<void (Credential&&)> completionHandler)
{
#if USE(LIBSECRET)
    if (m_sessionID.isEphemeral()) {
        completionHandler({ });
        return;
    }

    const String& realm = protectionSpace.realm();
    if (realm.isEmpty()) {
        completionHandler({ });
        return;
    }

    GRefPtr<GHashTable> attributes = adoptGRef(secret_attributes_build(SECRET_SCHEMA_COMPAT_NETWORK,
        "domain", realm.utf8().data(),
        "server", protectionSpace.host().utf8().data(),
        "port", protectionSpace.port(),
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()),
        nullptr));
    if (!attributes) {
        completionHandler({ });
        return;
    }

    m_persisentStorageCancellable = adoptGRef(g_cancellable_new());
    m_persisentStorageCompletionHandler = WTFMove(completionHandler);
    secret_service_search(nullptr, SECRET_SCHEMA_COMPAT_NETWORK, attributes.get(),
        static_cast<SecretSearchFlags>(SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS), m_persisentStorageCancellable.get(),
        [](GObject* source, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GUniquePtr<GList> elements(secret_service_search_finish(SECRET_SERVICE(source), result, &error.outPtr()));
            if (g_error_matches (error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            NetworkStorageSession* session = static_cast<NetworkStorageSession*>(userData);
            auto completionHandler = std::exchange(session->m_persisentStorageCompletionHandler, nullptr);
            if (error || !elements || !elements->data) {
                completionHandler({ });
                return;
            }

            GRefPtr<SecretItem> secretItem = adoptGRef(static_cast<SecretItem*>(elements->data));
            GRefPtr<GHashTable> attributes = adoptGRef(secret_item_get_attributes(secretItem.get()));
            String user = String::fromUTF8(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "user")));
            if (user.isEmpty()) {
                completionHandler({ });
                return;
            }

            size_t length;
            GRefPtr<SecretValue> secretValue = adoptGRef(secret_item_get_secret(secretItem.get()));
            const char* passwordData = secret_value_get(secretValue.get(), &length);
            completionHandler(Credential(user, String::fromUTF8(passwordData, length), CredentialPersistencePermanent));
    }, this);
#else
    UNUSED_PARAM(protectionSpace);
    completionHandler({ });
#endif
}

void NetworkStorageSession::saveCredentialToPersistentStorage(const ProtectionSpace& protectionSpace, const Credential& credential)
{
#if USE(LIBSECRET)
    if (m_sessionID.isEphemeral())
        return;

    if (credential.isEmpty())
        return;

    const String& realm = protectionSpace.realm();
    if (realm.isEmpty())
        return;

    GRefPtr<GHashTable> attributes = adoptGRef(secret_attributes_build(SECRET_SCHEMA_COMPAT_NETWORK,
        "domain", realm.utf8().data(),
        "server", protectionSpace.host().utf8().data(),
        "port", protectionSpace.port(),
        "protocol", schemeFromProtectionSpaceServerType(protectionSpace.serverType()),
        "authtype", authTypeFromProtectionSpaceAuthenticationScheme(protectionSpace.authenticationScheme()),
        nullptr));
    if (!attributes)
        return;

    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(credential.user().utf8().data()));
    CString utf8Password = credential.password().utf8();
    GRefPtr<SecretValue> newSecretValue = adoptGRef(secret_value_new(utf8Password.data(), utf8Password.length(), "text/plain"));
    secret_service_store(nullptr, SECRET_SCHEMA_COMPAT_NETWORK, attributes.get(), SECRET_COLLECTION_DEFAULT, _("WebKitGTK+ password"),
        newSecretValue.get(), nullptr, nullptr, nullptr);
#else
    UNUSED_PARAM(protectionSpace);
    UNUSED_PARAM(credential);
#endif
}

} // namespace WebCore

#endif // USE(SOUP)
