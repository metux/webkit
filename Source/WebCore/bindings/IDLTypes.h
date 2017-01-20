/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <heap/HandleTypes.h>
#include <wtf/Brigand.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "WebGLAny.h"
#endif

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
class JSValue;
class JSObject;
template<typename> class Strong;
}

namespace WebCore {

class IDBKey;
template<typename> class DOMPromise;

template<typename T>
struct IDLType {
    using ImplementationType = T;
    using ParameterType = T;

    using NullableType = std::optional<ImplementationType>;
    static NullableType nullValue() { return std::nullopt; }
    static bool isNullValue(const NullableType& value) { return !value; }
    static ImplementationType extractValueFromNullable(const NullableType& value) { return value.value(); }
};

// IDLUnsupportedType is a special type that serves as a base class for currently unsupported types.
struct IDLUnsupportedType : IDLType<void> { };

// IDLNull is a special type for use as a subtype in an IDLUnion that is nullable.
struct IDLNull : IDLType<std::nullptr_t> { };

struct IDLAny : IDLType<JSC::Strong<JSC::Unknown>> {
    using ParameterType = JSC::JSValue;

    using NullableType = JSC::Strong<JSC::Unknown>;
    static inline std::nullptr_t nullValue() { return nullptr; }
    template<typename U> static inline bool isNullValue(U&& value) { return !value; }
    template<typename U> static inline U&& extractValueFromNullable(U&& value) { return std::forward<U>(value); }
};

struct IDLBoolean : IDLType<bool> { };

template<typename NumericType> struct IDLNumber : IDLType<NumericType> { };

template<typename IntegerType> struct IDLInteger : IDLNumber<IntegerType> { };
struct IDLByte : IDLInteger<int8_t> { };
struct IDLOctet : IDLInteger<uint8_t> { };
struct IDLShort : IDLInteger<int16_t> { };
struct IDLUnsignedShort : IDLInteger<uint16_t> { };
struct IDLLong : IDLInteger<int32_t> { };
struct IDLUnsignedLong : IDLInteger<uint32_t> { };
struct IDLLongLong : IDLInteger<int64_t> { };
struct IDLUnsignedLongLong : IDLInteger<uint64_t> { };

template<typename FloatingPointType> struct IDLFloatingPoint : IDLNumber<FloatingPointType> { };
struct IDLFloat : IDLFloatingPoint<float> { };
struct IDLUnrestrictedFloat : IDLFloatingPoint<float> { };
struct IDLDouble : IDLFloatingPoint<double> { };
struct IDLUnrestrictedDouble : IDLFloatingPoint<double> { };

struct IDLString : IDLType<String> {
    using ParameterType = const String&;

    using NullableType = String;
    static String nullValue() { return String(); }
    static bool isNullValue(const String& value) { return value.isNull(); }
    template <typename U> static U&& extractValueFromNullable(U&& value) { return std::forward<U>(value); }
};
struct IDLDOMString : IDLString { };
struct IDLByteString : IDLString { };
struct IDLUSVString : IDLString { };

struct IDLObject : IDLType<JSC::Strong<JSC::JSObject>> {
    using NullableType = JSC::Strong<JSC::JSObject>;

    static inline std::nullptr_t nullValue() { return nullptr; }
    template<typename U> static inline bool isNullValue(U&& value) { return !value; }
    template<typename U> static inline U&& extractValueFromNullable(U&& value) { return std::forward<U>(value); }
};

template<typename T> struct IDLWrapper : IDLType<RefPtr<T>> {
    using RawType = T;

    using ParameterType = T&;

    using NullableType = RefPtr<T>;
    static inline std::nullptr_t nullValue() { return nullptr; }
    template<typename U> static inline bool isNullValue(U&& value) { return !value; }
    template<typename U> static inline U&& extractValueFromNullable(U&& value) { return std::forward<U>(value); }
};

template<typename T> struct IDLInterface : IDLWrapper<T> { };
template<typename T> struct IDLCallbackInterface : IDLWrapper<T> { };
template<typename T> struct IDLCallbackFunction : IDLWrapper<T> { };

template<typename T> struct IDLDictionary : IDLType<T> {
    using ParameterType = const T&;
};

template<typename T> struct IDLEnumeration : IDLType<T> { };

template<typename T> struct IDLNullable : IDLType<typename T::NullableType> {
    using InnerType = T;

    using NullableType = typename T::NullableType;
    static inline auto nullValue() -> decltype(T::nullValue()) { return T::nullValue(); }
    template<typename U> static inline bool isNullValue(U&& value) { return T::isNullValue(std::forward<U>(value)); }
    template<typename U> static inline auto extractValueFromNullable(U&& value) -> decltype(T::extractValueFromNullable(std::forward<U>(value))) { return T::extractValueFromNullable(std::forward<U>(value)); }
};

template<typename T> struct IDLSequence : IDLType<Vector<typename T::ImplementationType>> {
    using InnerType = T;

    using ParameterType = const Vector<typename T::ImplementationType>&;
};

template<typename T> struct IDLFrozenArray : IDLType<Vector<typename T::ImplementationType>> {
    using InnerType = T;

    using ParameterType = const Vector<typename T::ImplementationType>&;
};

template<typename K, typename V> struct IDLRecord : IDLType<Vector<WTF::KeyValuePair<typename K::ImplementationType, typename V::ImplementationType>>> {
    using KeyType = K;
    using ValueType = V;

    using ParameterType = const Vector<WTF::KeyValuePair<typename K::ImplementationType, typename V::ImplementationType>>&;
};

template<typename T> struct IDLPromise : IDLType<DOMPromise<T>> {
    using InnerType = T;
};

struct IDLError : IDLUnsupportedType { };
struct IDLDOMException : IDLUnsupportedType { };

template<typename... Ts>
struct IDLUnion : IDLType<Variant<typename Ts::ImplementationType...>> {
    using TypeList = brigand::list<Ts...>;

    using ParameterType = const Variant<typename Ts::ImplementationType...>&;
};

// Non-WebIDL extensions

struct IDLDate : IDLType<double> { 
    using NullableType = double;
    static double nullValue() { return std::numeric_limits<double>::quiet_NaN(); }
    static bool isNullValue(double value) { return std::isnan(value); }
    static double extractValueFromNullable(double value) { return value; }
};

struct IDLJSON : IDLType<String> { 
    using ParameterType = const String&;

    using NullableType = String;
    static String nullValue() { return String(); }
    static bool isNullValue(const String& value) { return value.isNull(); }
    template <typename U> static U&& extractValueFromNullable(U&& value) { return std::forward<U>(value); }
};

template<typename T> struct IDLSerializedScriptValue : IDLWrapper<T> { };
template<typename T> struct IDLEventListener : IDLWrapper<T> { };
template<typename T> struct IDLXPathNSResolver : IDLWrapper<T> { };

struct IDLIDBKey : IDLWrapper<IDBKey> { };

#if ENABLE(WEBGL)
struct IDLWebGLAny : IDLType<WebGLAny> { };
#endif

// Non-WebIDL convenience type aliases

using IDLBufferSource = IDLUnion<IDLInterface<JSC::ArrayBufferView>, IDLInterface<JSC::ArrayBuffer>>;


// Helper predicates

template<typename T>
struct IsIDLInterface : public std::integral_constant<bool, WTF::IsTemplate<T, IDLInterface>::value> { };

template<typename T>
struct IsIDLDictionary : public std::integral_constant<bool, WTF::IsTemplate<T, IDLDictionary>::value> { };

template<typename T>
struct IsIDLEnumeration : public std::integral_constant<bool, WTF::IsTemplate<T, IDLEnumeration>::value> { };

template<typename T>
struct IsIDLSequence : public std::integral_constant<bool, WTF::IsTemplate<T, IDLSequence>::value> { };

template<typename T>
struct IsIDLFrozenArray : public std::integral_constant<bool, WTF::IsTemplate<T, IDLFrozenArray>::value> { };

template<typename T>
struct IsIDLRecord : public std::integral_constant<bool, WTF::IsTemplate<T, IDLRecord>::value> { };

template<typename T>
struct IsIDLNumber : public std::integral_constant<bool, WTF::IsBaseOfTemplate<IDLNumber, T>::value> { };

template<typename T>
struct IsIDLInteger : public std::integral_constant<bool, WTF::IsBaseOfTemplate<IDLInteger, T>::value> { };

template<typename T>
struct IsIDLFloatingPoint : public std::integral_constant<bool, WTF::IsBaseOfTemplate<IDLFloatingPoint, T>::value> { };

} // namespace WebCore
