/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSDOMConvertNumbers.h"

#include "JSDOMExceptionHandling.h"
#include <heap/HeapInlines.h>
#include <runtime/JSCJSValueInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

static const int32_t kMaxInt32 = 0x7fffffff;
static const int32_t kMinInt32 = -kMaxInt32 - 1;
static const uint32_t kMaxUInt32 = 0xffffffffU;
static const int64_t kJSMaxInteger = 0x20000000000000LL - 1; // 2^53 - 1, largest integer exactly representable in ECMAScript.

static String rangeErrorString(double value, double min, double max)
{
    return makeString("Value ", String::numberToStringECMAScript(value), " is outside the range [", String::numberToStringECMAScript(min), ", ", String::numberToStringECMAScript(max), "]");
}

static double enforceRange(ExecState& state, double x, double minimum, double maximum)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::isnan(x) || std::isinf(x)) {
        throwTypeError(&state, scope, rangeErrorString(x, minimum, maximum));
        return 0;
    }
    x = trunc(x);
    if (x < minimum || x > maximum) {
        throwTypeError(&state, scope, rangeErrorString(x, minimum, maximum));
        return 0;
    }
    return x;
}

template <typename T>
struct IntTypeLimits {
};

template <>
struct IntTypeLimits<int8_t> {
    static const int8_t minValue = -128;
    static const int8_t maxValue = 127;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
    static const uint8_t maxValue = 255;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
    static const short minValue = -32768;
    static const short maxValue = 32767;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
    static const unsigned short maxValue = 65535;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <typename T>
static inline T toSmallerInt(ExecState& state, JSValue value, IntegerConversionConfiguration configuration)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(std::is_signed<T>::value && std::is_integral<T>::value, "Should only be used for signed integral types");

    typedef IntTypeLimits<T> LimitsTrait;
    // Fast path if the value is already a 32-bit signed integer in the right range.
    if (value.isInt32()) {
        int32_t d = value.asInt32();
        if (d >= LimitsTrait::minValue && d <= LimitsTrait::maxValue)
            return static_cast<T>(d);
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            throwTypeError(&state, scope);
            return 0;
        case IntegerConversionConfiguration::Clamp:
            return d < LimitsTrait::minValue ? LimitsTrait::minValue : LimitsTrait::maxValue;
        }
        d %= LimitsTrait::numberOfValues;
        return static_cast<T>(d > LimitsTrait::maxValue ? d - LimitsTrait::numberOfValues : d);
    }

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);

    switch (configuration) {
    case IntegerConversionConfiguration::Normal:
        break;
    case IntegerConversionConfiguration::EnforceRange:
        return enforceRange(state, x, LimitsTrait::minValue, LimitsTrait::maxValue);
    case IntegerConversionConfiguration::Clamp:
        return std::isnan(x) ? 0 : clampTo<T>(x);
    }

    if (std::isnan(x) || std::isinf(x) || !x)
        return 0;

    x = x < 0 ? -floor(fabs(x)) : floor(fabs(x));
    x = fmod(x, LimitsTrait::numberOfValues);

    return static_cast<T>(x > LimitsTrait::maxValue ? x - LimitsTrait::numberOfValues : x);
}

template <typename T>
static inline T toSmallerUInt(ExecState& state, JSValue value, IntegerConversionConfiguration configuration)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(std::is_unsigned<T>::value && std::is_integral<T>::value, "Should only be used for unsigned integral types");

    typedef IntTypeLimits<T> LimitsTrait;
    // Fast path if the value is already a 32-bit unsigned integer in the right range.
    if (value.isUInt32()) {
        uint32_t d = value.asUInt32();
        if (d <= LimitsTrait::maxValue)
            return static_cast<T>(d);
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            return static_cast<T>(d);
        case IntegerConversionConfiguration::EnforceRange:
            throwTypeError(&state, scope);
            return 0;
        case IntegerConversionConfiguration::Clamp:
            return LimitsTrait::maxValue;
        }
    }

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);

    switch (configuration) {
    case IntegerConversionConfiguration::Normal:
        break;
    case IntegerConversionConfiguration::EnforceRange:
        return enforceRange(state, x, 0, LimitsTrait::maxValue);
    case IntegerConversionConfiguration::Clamp:
        return std::isnan(x) ? 0 : clampTo<T>(x);
    }

    if (std::isnan(x) || std::isinf(x) || !x)
        return 0;

    x = x < 0 ? -floor(fabs(x)) : floor(fabs(x));
    return static_cast<T>(fmod(x, LimitsTrait::numberOfValues));
}

int8_t toInt8EnforceRange(JSC::ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

uint8_t toUInt8EnforceRange(JSC::ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

int8_t toInt8Clamp(JSC::ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::Clamp);
}

uint8_t toUInt8Clamp(JSC::ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::Clamp);
}

// http://www.w3.org/TR/WebIDL/#es-byte
int8_t toInt8(ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-octet
uint8_t toUInt8(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::Normal);
}

int16_t toInt16EnforceRange(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

uint16_t toUInt16EnforceRange(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

int16_t toInt16Clamp(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::Clamp);
}

uint16_t toUInt16Clamp(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::Clamp);
}

// http://www.w3.org/TR/WebIDL/#es-short
int16_t toInt16(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-short
uint16_t toUInt16(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-long
int32_t toInt32EnforceRange(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isInt32())
        return value.asInt32();

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, kMinInt32, kMaxInt32);
}

int32_t toInt32Clamp(ExecState& state, JSValue value)
{
    if (value.isInt32())
        return value.asInt32();

    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : clampTo<int32_t>(x);
}

uint32_t toUInt32Clamp(ExecState& state, JSValue value)
{
    if (value.isUInt32())
        return value.asUInt32();

    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : clampTo<uint32_t>(x);
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-long
uint32_t toUInt32EnforceRange(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isUInt32())
        return value.asUInt32();

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, 0, kMaxUInt32);
}

int64_t toInt64EnforceRange(ExecState& state, JSC::JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, -kJSMaxInteger, kJSMaxInteger);
}

uint64_t toUInt64EnforceRange(ExecState& state, JSC::JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, 0, kJSMaxInteger);
}

int64_t toInt64Clamp(ExecState& state, JSC::JSValue value)
{
    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : static_cast<int64_t>(std::min<double>(std::max<double>(x, -kJSMaxInteger), kJSMaxInteger));
}

uint64_t toUInt64Clamp(ExecState& state, JSC::JSValue value)
{
    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : static_cast<uint64_t>(std::min<double>(std::max<double>(x, 0), kJSMaxInteger));
}

// http://www.w3.org/TR/WebIDL/#es-long-long
int64_t toInt64(ExecState& state, JSValue value)
{
    double x = value.toNumber(&state);

    // Map NaNs and +/-Infinity to 0; convert finite values modulo 2^64.
    unsigned long long n;
    doubleToInteger(x, n);
    return n;
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
uint64_t toUInt64(ExecState& state, JSValue value)
{
    double x = value.toNumber(&state);

    // Map NaNs and +/-Infinity to 0; convert finite values modulo 2^64.
    unsigned long long n;
    doubleToInteger(x, n);
    return n;
}

} // namespace WebCore
