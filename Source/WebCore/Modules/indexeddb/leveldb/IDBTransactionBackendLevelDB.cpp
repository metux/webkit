/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "IDBTransactionBackendLevelDB.h"

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreLevelDB.h"
#include "IDBCursorBackendLevelDB.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRange.h"
#include "IDBTransactionBackendLevelDBOperations.h"
#include "IDBTransactionCoordinator.h"
#include "Logging.h"

namespace WebCore {

PassRefPtr<IDBTransactionBackendLevelDB> IDBTransactionBackendLevelDB::create(IDBDatabaseBackendImpl* databaseBackend, int64_t id, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, IndexedDB::TransactionMode mode)
{
    HashSet<int64_t> objectStoreHashSet;
    for (size_t i = 0; i < objectStoreIds.size(); ++i)
        objectStoreHashSet.add(objectStoreIds[i]);

    return adoptRef(new IDBTransactionBackendLevelDB(databaseBackend, id, callbacks, objectStoreHashSet, mode));
}

IDBTransactionBackendLevelDB::IDBTransactionBackendLevelDB(IDBDatabaseBackendImpl* databaseBackend, int64_t id, PassRefPtr<IDBDatabaseCallbacks> callbacks, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode mode)
    : IDBTransactionBackendInterface(id)
    , m_objectStoreIds(objectStoreIds)
    , m_mode(mode)
    , m_state(Unused)
    , m_commitPending(false)
    , m_callbacks(callbacks)
    , m_database(databaseBackend)
    , m_transaction(databaseBackend->backingStore())
    , m_taskTimer(this, &IDBTransactionBackendLevelDB::taskTimerFired)
    , m_pendingPreemptiveEvents(0)
    , m_backingStore(databaseBackend->backingStore())
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();

    m_database->transactionCoordinator()->didCreateTransaction(this);
}

IDBTransactionBackendLevelDB::~IDBTransactionBackendLevelDB()
{
    // It shouldn't be possible for this object to get deleted until it's either complete or aborted.
    ASSERT(m_state == Finished);
}

void IDBTransactionBackendLevelDB::scheduleTask(IDBDatabaseBackendInterface::TaskType type, PassOwnPtr<Operation> task, PassOwnPtr<Operation> abortTask)
{
    if (m_state == Finished)
        return;

    if (type == IDBDatabaseBackendInterface::NormalTask)
        m_taskQueue.append(task);
    else
        m_preemptiveTaskQueue.append(task);

    if (abortTask)
        m_abortTaskQueue.prepend(abortTask);

    if (m_state == Unused)
        start();
    else if (m_state == Running && !m_taskTimer.isActive())
        m_taskTimer.startOneShot(0);
}

void IDBTransactionBackendLevelDB::abort()
{
    abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error (unknown cause)"));
}

void IDBTransactionBackendLevelDB::abort(PassRefPtr<IDBDatabaseError> error)
{
    LOG(StorageAPI, "IDBTransactionBackendLevelDB::abort");
    if (m_state == Finished)
        return;

    bool wasRunning = m_state == Running;

    // The last reference to this object may be released while performing the
    // abort steps below. We therefore take a self reference to keep ourselves
    // alive while executing this method.
    Ref<IDBTransactionBackendLevelDB> protect(*this);

    m_state = Finished;
    m_taskTimer.stop();

    if (wasRunning)
        m_transaction.rollback();

    // Run the abort tasks, if any.
    while (!m_abortTaskQueue.isEmpty()) {
        OwnPtr<Operation> task(m_abortTaskQueue.takeFirst());
        task->perform();
    }

    // Backing store resources (held via cursors) must be released before script callbacks
    // are fired, as the script callbacks may release references and allow the backing store
    // itself to be released, and order is critical.
    closeOpenCursors();
    m_transaction.reset();

    // Transactions must also be marked as completed before the front-end is notified, as
    // the transaction completion unblocks operations like closing connections.
    m_database->transactionCoordinator()->didFinishTransaction(this);
    ASSERT(!m_database->transactionCoordinator()->isActive(this));
    m_database->transactionFinished(this);

    if (m_callbacks)
        m_callbacks->onAbort(id(), error);

    m_database->transactionFinishedAndAbortFired(this);

    m_database = 0;
}

bool IDBTransactionBackendLevelDB::isTaskQueueEmpty() const
{
    return m_preemptiveTaskQueue.isEmpty() && m_taskQueue.isEmpty();
}

bool IDBTransactionBackendLevelDB::hasPendingTasks() const
{
    return m_pendingPreemptiveEvents || !isTaskQueueEmpty();
}

void IDBTransactionBackendLevelDB::registerOpenCursor(IDBCursorBackendLevelDB* cursor)
{
    m_openCursors.add(cursor);
}

void IDBTransactionBackendLevelDB::unregisterOpenCursor(IDBCursorBackendLevelDB* cursor)
{
    m_openCursors.remove(cursor);
}

void IDBTransactionBackendLevelDB::run()
{
    // TransactionCoordinator has started this transaction. Schedule a timer
    // to process the first task.
    ASSERT(m_state == StartPending || m_state == Running);
    ASSERT(!m_taskTimer.isActive());

    m_taskTimer.startOneShot(0);
}

void IDBTransactionBackendLevelDB::start()
{
    ASSERT(m_state == Unused);

    m_state = StartPending;
    m_database->transactionCoordinator()->didStartTransaction(this);
    m_database->transactionStarted(this);
}

void IDBTransactionBackendLevelDB::commit()
{
    LOG(StorageAPI, "IDBTransactionBackendLevelDB::commit");

    // In multiprocess ports, front-end may have requested a commit but an abort has already
    // been initiated asynchronously by the back-end.
    if (m_state == Finished)
        return;

    ASSERT(m_state == Unused || m_state == Running);
    m_commitPending = true;

    // Front-end has requested a commit, but there may be tasks like createIndex which
    // are considered synchronous by the front-end but are processed asynchronously.
    if (hasPendingTasks())
        return;

    // The last reference to this object may be released while performing the
    // commit steps below. We therefore take a self reference to keep ourselves
    // alive while executing this method.
    Ref<IDBTransactionBackendLevelDB> protect(*this);

    bool unused = m_state == Unused;
    m_state = Finished;

    bool committed = unused || m_transaction.commit();

    // Backing store resources (held via cursors) must be released before script callbacks
    // are fired, as the script callbacks may release references and allow the backing store
    // itself to be released, and order is critical.
    closeOpenCursors();
    m_transaction.reset();

    // Transactions must also be marked as completed before the front-end is notified, as
    // the transaction completion unblocks operations like closing connections.
    if (!unused)
        m_database->transactionCoordinator()->didFinishTransaction(this);
    m_database->transactionFinished(this);

    if (committed) {
        m_callbacks->onComplete(id());
        m_database->transactionFinishedAndCompleteFired(this);
    } else {
        m_callbacks->onAbort(id(), IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error committing transaction."));
        m_database->transactionFinishedAndAbortFired(this);
    }

    m_database = 0;
}

void IDBTransactionBackendLevelDB::taskTimerFired(Timer<IDBTransactionBackendLevelDB>*)
{
    LOG(StorageAPI, "IDBTransactionBackendLevelDB::taskTimerFired");
    ASSERT(!isTaskQueueEmpty());

    if (m_state == StartPending) {
        m_transaction.begin();
        m_state = Running;
    }

    // The last reference to this object may be released while performing the
    // tasks. Take take a self reference to keep this object alive so that
    // the loop termination conditions can be checked.
    Ref<IDBTransactionBackendLevelDB> protect(*this);

    TaskQueue* taskQueue = m_pendingPreemptiveEvents ? &m_preemptiveTaskQueue : &m_taskQueue;
    while (!taskQueue->isEmpty() && m_state != Finished) {
        ASSERT(m_state == Running);
        OwnPtr<Operation> task(taskQueue->takeFirst());
        task->perform();

        // Event itself may change which queue should be processed next.
        taskQueue = m_pendingPreemptiveEvents ? &m_preemptiveTaskQueue : &m_taskQueue;
    }

    // If there are no pending tasks, we haven't already committed/aborted,
    // and the front-end requested a commit, it is now safe to do so.
    if (!hasPendingTasks() && m_state != Finished && m_commitPending)
        commit();
}

void IDBTransactionBackendLevelDB::closeOpenCursors()
{
    for (HashSet<IDBCursorBackendLevelDB*>::iterator i = m_openCursors.begin(); i != m_openCursors.end(); ++i)
        (*i)->close();
    m_openCursors.clear();
}

void IDBTransactionBackendLevelDB::scheduleCreateObjectStoreOperation(const IDBObjectStoreMetadata& objectStoreMetadata)
{
    scheduleTask(CreateObjectStoreOperation::create(this, m_backingStore.get(), objectStoreMetadata), CreateObjectStoreAbortOperation::create(this, objectStoreMetadata.id));
}

void IDBTransactionBackendLevelDB::scheduleDeleteObjectStoreOperation(const IDBObjectStoreMetadata& objectStoreMetadata)
{
    scheduleTask(DeleteObjectStoreOperation::create(this, m_backingStore.get(), objectStoreMetadata), DeleteObjectStoreAbortOperation::create(this, objectStoreMetadata));
}

void IDBTransactionBackendLevelDB::scheduleVersionChangeOperation(int64_t transactionId, int64_t requestedVersion, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, const IDBDatabaseMetadata& metadata)
{
    scheduleTask(IDBDatabaseBackendImpl::VersionChangeOperation::create(this, transactionId, requestedVersion, callbacks, databaseCallbacks), IDBDatabaseBackendImpl::VersionChangeAbortOperation::create(this, String::number(metadata.version), metadata.version));
}

void IDBTransactionBackendLevelDB::scheduleCreateIndexOperation(int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
{
    scheduleTask(CreateIndexOperation::create(this, m_backingStore.get(), objectStoreId, indexMetadata), CreateIndexAbortOperation::create(this, objectStoreId, indexMetadata.id));
}

void IDBTransactionBackendLevelDB::scheduleDeleteIndexOperation(int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
{
    scheduleTask(DeleteIndexOperation::create(this, m_backingStore.get(), objectStoreId, indexMetadata), DeleteIndexAbortOperation::create(this, objectStoreId, indexMetadata));
}

void IDBTransactionBackendLevelDB::scheduleGetOperation(const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(GetOperation::create(this, m_backingStore.get(), metadata, objectStoreId, indexId, keyRange, cursorType, callbacks));
}

void IDBTransactionBackendLevelDB::schedulePutOperation(const IDBObjectStoreMetadata& objectStoreMetadata, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, IDBDatabaseBackendInterface::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    scheduleTask(PutOperation::create(this, m_backingStore.get(), database()->id(), objectStoreMetadata, value, key, putMode, callbacks, indexIds, indexKeys));
}

void IDBTransactionBackendLevelDB::scheduleSetIndexesReadyOperation(size_t indexCount)
{
    scheduleTask(IDBDatabaseBackendInterface::PreemptiveTask, SetIndexesReadyOperation::create(this, indexCount));
}

void IDBTransactionBackendLevelDB::scheduleOpenCursorOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackendInterface::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(OpenCursorOperation::create(this, m_backingStore.get(), database()->id(), objectStoreId, indexId, keyRange, direction, cursorType, taskType, callbacks));
}

void IDBTransactionBackendLevelDB::scheduleCountOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(CountOperation::create(this, m_backingStore.get(), database()->id(), objectStoreId, indexId, keyRange, callbacks));
}

void IDBTransactionBackendLevelDB::scheduleDeleteRangeOperation(int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(DeleteRangeOperation::create(this, m_backingStore.get(), database()->id(), objectStoreId, keyRange, callbacks));
}

void IDBTransactionBackendLevelDB::scheduleClearOperation(int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    scheduleTask(ClearOperation::create(this, m_backingStore.get(), database()->id(), objectStoreId, callbacks));
}


};

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)