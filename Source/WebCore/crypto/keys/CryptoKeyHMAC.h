/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef CryptoKeyHMAC_h
#define CryptoKeyHMAC_h

#include "CryptoKey.h"
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class HmacKeyAlgorithm final : public KeyAlgorithm {
public:
    HmacKeyAlgorithm(const String& name, const String& hash, size_t length)
        : KeyAlgorithm(name)
        , m_hash(hash)
        , m_length(length)
    {
    }

    KeyAlgorithmClass keyAlgorithmClass() const final { return KeyAlgorithmClass::HMAC; }

    const String& hash() const { return m_hash; }
    size_t length() const { return m_length; }

private:
    String m_hash;
    size_t m_length;
};

class CryptoKeyHMAC final : public CryptoKey {
public:
    static Ref<CryptoKeyHMAC> create(const Vector<uint8_t>& key, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage usage)
    {
        return adoptRef(*new CryptoKeyHMAC(key, hash, extractable, usage));
    }
    virtual ~CryptoKeyHMAC();

    // If lengthBytes is 0, a recommended length is used, which is the size of the associated hash function's block size.
    static RefPtr<CryptoKeyHMAC> generate(size_t lengthBytes, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage);

    CryptoKeyClass keyClass() const final { return CryptoKeyClass::HMAC; }

    const Vector<uint8_t>& key() const { return m_key; }

    CryptoAlgorithmIdentifier hashAlgorithmIdentifier() const { return m_hash; }

private:
    CryptoKeyHMAC(const Vector<uint8_t>& key, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage);

    std::unique_ptr<KeyAlgorithm> buildAlgorithm() const final;
    std::unique_ptr<CryptoKeyData> exportData() const final;

    CryptoAlgorithmIdentifier m_hash;
    Vector<uint8_t> m_key;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CRYPTO_KEY(CryptoKeyHMAC, CryptoKeyClass::HMAC)

SPECIALIZE_TYPE_TRAITS_KEY_ALGORITHM(HmacKeyAlgorithm, KeyAlgorithmClass::HMAC)

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeyHMAC_h
