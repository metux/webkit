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

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
#include "NetworkCacheSubresourcesEntry.h"

#include "Logging.h"
#include "NetworkCacheCoders.h"

namespace WebKit {
namespace NetworkCache {

void SubresourceInfo::encode(WTF::Persistence::Encoder& encoder) const
{
    encoder << m_key;
    encoder << m_isTransient;

    // Do not bother serializing other data members of transient resources as they are empty.
    if (m_isTransient)
        return;

    encoder << m_firstPartyForCookies;
    encoder << m_requestHeaders;
    encoder.encodeEnum(m_priority);
}

bool SubresourceInfo::decode(WTF::Persistence::Decoder& decoder, SubresourceInfo& info)
{
    if (!decoder.decode(info.m_key))
        return false;

    if (!decoder.decode(info.m_isTransient))
        return false;

    if (info.m_isTransient)
        return true;

    if (!decoder.decode(info.m_firstPartyForCookies))
        return false;

    if (!decoder.decode(info.m_requestHeaders))
        return false;

    if (!decoder.decodeEnum(info.m_priority))
        return false;
    
    return true;
}

Storage::Record SubresourcesEntry::encodeAsStorageRecord() const
{
    WTF::Persistence::Encoder encoder;
    encoder << m_subresources;

    encoder.encodeChecksum();

    return { m_key, m_timeStamp, { encoder.buffer(), encoder.bufferSize() }, { }, { }};
}

std::unique_ptr<SubresourcesEntry> SubresourcesEntry::decodeStorageRecord(const Storage::Record& storageEntry)
{
    auto entry = std::make_unique<SubresourcesEntry>(storageEntry);

    WTF::Persistence::Decoder decoder(storageEntry.header.data(), storageEntry.header.size());
    if (!decoder.decode(entry->m_subresources))
        return nullptr;

    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        return nullptr;
    }

    return entry;
}

SubresourcesEntry::SubresourcesEntry(const Storage::Record& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
{
    ASSERT(m_key.type() == "SubResources");
}
    
static Vector<SubresourceInfo> makeSubresourceInfoVector(const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads)
{
    Vector<SubresourceInfo> result;
    result.reserveInitialCapacity(subresourceLoads.size());
    
    HashSet<Key> seenKeys;
    for (auto& load : subresourceLoads) {
        if (!seenKeys.add(load->key).isNewEntry)
            continue;
        result.uncheckedAppend({ load->key, load->request });
    }

    return result;
}

SubresourcesEntry::SubresourcesEntry(Key&& key, const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads)
    : m_key(WTFMove(key))
    , m_timeStamp(std::chrono::system_clock::now())
    , m_subresources(makeSubresourceInfoVector(subresourceLoads))
{
    ASSERT(m_key.type() == "SubResources");
}

void SubresourcesEntry::updateSubresourceLoads(const Vector<std::unique_ptr<SubresourceLoad>>& subresourceLoads)
{
    HashSet<Key> previousKeys;
    for (auto& info : m_subresources)
        previousKeys.add(info.key());
    
    m_subresources = makeSubresourceInfoVector(subresourceLoads);

    // Mark keys that are not common with the last load as transient.
    for (auto& subresourceInfo : m_subresources) {
        if (!previousKeys.contains(subresourceInfo.key()))
            subresourceInfo.setTransient();
    }
}

} // namespace WebKit
} // namespace NetworkCache

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
