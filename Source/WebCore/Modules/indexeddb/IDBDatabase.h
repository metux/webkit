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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "Dictionary.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "IDBActiveDOMObject.h"
#include "IDBConnectionProxy.h"
#include "IDBConnectionToServer.h"
#include "IDBDatabaseInfo.h"
#include "IDBKeyPath.h"

namespace WebCore {

class DOMStringList;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBResultData;
class IDBTransaction;
class IDBTransactionInfo;

class IDBDatabase : public ThreadSafeRefCounted<IDBDatabase>, public EventTargetWithInlineData, public IDBActiveDOMObject {
public:
    static Ref<IDBDatabase> create(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&, const IDBResultData&);

    virtual ~IDBDatabase();

    // IDBDatabase IDL
    const String name() const;
    uint64_t version() const;
    RefPtr<DOMStringList> objectStoreNames() const;

    struct ObjectStoreParameters {
        Optional<IDBKeyPathVariant> keyPath;
        bool autoIncrement;
    };

    ExceptionOr<Ref<IDBObjectStore>> createObjectStore(const String& name, ObjectStoreParameters&&);

    using StringOrVectorOfStrings = WTF::Variant<String, Vector<String>>;
    ExceptionOr<Ref<IDBTransaction>> transaction(StringOrVectorOfStrings&& storeNames, const String& mode);
    ExceptionOr<void> deleteObjectStore(const String& name);
    void close();

    void renameObjectStore(IDBObjectStore&, const String& newName);
    void renameIndex(IDBIndex&, const String& newName);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return IDBDatabaseEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ThreadSafeRefCounted<IDBDatabase>::ref(); }
    void derefEventTarget() final { ThreadSafeRefCounted<IDBDatabase>::deref(); }

    using ThreadSafeRefCounted<IDBDatabase>::ref;
    using ThreadSafeRefCounted<IDBDatabase>::deref;

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;

    const IDBDatabaseInfo& info() const { return m_info; }
    uint64_t databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }

    Ref<IDBTransaction> startVersionChangeTransaction(const IDBTransactionInfo&, IDBOpenDBRequest&);
    void didStartTransaction(IDBTransaction&);

    void willCommitTransaction(IDBTransaction&);
    void didCommitTransaction(IDBTransaction&);
    void willAbortTransaction(IDBTransaction&);
    void didAbortTransaction(IDBTransaction&);

    void fireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didCloseFromServer(const IDBError&);
    void connectionToServerLost(const IDBError&);

    IDBClient::IDBConnectionProxy& connectionProxy() { return m_connectionProxy.get(); }

    void didCreateIndexInfo(const IDBIndexInfo&);
    void didDeleteIndexInfo(const IDBIndexInfo&);

    bool isClosingOrClosed() const { return m_closePending || m_closedInServer; }

    bool dispatchEvent(Event&) final;

    bool hasPendingActivity() const final;

private:
    IDBDatabase(ScriptExecutionContext&, IDBClient::IDBConnectionProxy&, const IDBResultData&);

    void didCommitOrAbortTransaction(IDBTransaction&);

    void maybeCloseInServer();

    Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;
    IDBDatabaseInfo m_info;
    uint64_t m_databaseConnectionIdentifier { 0 };

    bool m_closePending { false };
    bool m_closedInServer { false };

    RefPtr<IDBTransaction> m_versionChangeTransaction;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_activeTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_abortingTransactions;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
