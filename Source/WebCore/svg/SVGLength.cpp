/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGLength.h"

#include "CSSHelper.h"
#include "CSSPrimitiveValue.h"
#include "ExceptionCode.h"
#include "FloatConversion.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "TextStream.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

namespace WebCore {

// Helper functions
static inline unsigned int storeUnit(SVGLengthMode mode, SVGLengthType type)
{
    return (mode << 4) | type;
}

static inline SVGLengthMode extractMode(unsigned int unit)
{
    unsigned int mode = unit >> 4;    
    return static_cast<SVGLengthMode>(mode);
}

static inline SVGLengthType extractType(unsigned int unit)
{
    unsigned int mode = unit >> 4;
    unsigned int type = unit ^ (mode << 4);
    return static_cast<SVGLengthType>(type);
}

static inline const char* lengthTypeToString(SVGLengthType type)
{
    switch (type) {
    case LengthTypeUnknown:
    case LengthTypeNumber:
        return "";    
    case LengthTypePercentage:
        return "%";
    case LengthTypeEMS:
        return "em";
    case LengthTypeEXS:
        return "ex";
    case LengthTypePX:
        return "px";
    case LengthTypeCM:
        return "cm";
    case LengthTypeMM:
        return "mm";
    case LengthTypeIN:
        return "in";
    case LengthTypePT:
        return "pt";
    case LengthTypePC:
        return "pc";
    }

    ASSERT_NOT_REACHED();
    return "";
}

inline SVGLengthType parseLengthType(const UChar* ptr, const UChar* end)
{
    if (ptr == end)
        return LengthTypeNumber;

    const UChar firstChar = *ptr;

    if (++ptr == end)
        return firstChar == '%' ? LengthTypePercentage : LengthTypeUnknown;

    const UChar secondChar = *ptr;

    if (++ptr != end)
        return LengthTypeUnknown;

    if (firstChar == 'e' && secondChar == 'm')
        return LengthTypeEMS;
    if (firstChar == 'e' && secondChar == 'x')
        return LengthTypeEXS;
    if (firstChar == 'p' && secondChar == 'x')
        return LengthTypePX;
    if (firstChar == 'c' && secondChar == 'm')
        return LengthTypeCM;
    if (firstChar == 'm' && secondChar == 'm')
        return LengthTypeMM;
    if (firstChar == 'i' && secondChar == 'n')
        return LengthTypeIN;
    if (firstChar == 'p' && secondChar == 't')
        return LengthTypePT;
    if (firstChar == 'p' && secondChar == 'c')
        return LengthTypePC;

    return LengthTypeUnknown;
}

SVGLength::SVGLength(SVGLengthMode mode, const String& valueAsString)
    : m_valueInSpecifiedUnits(0)
    , m_unit(storeUnit(mode, LengthTypeNumber))
{
    setValueAsString(valueAsString);
}

SVGLength::SVGLength(const SVGLengthContext& context, float value, SVGLengthMode mode, SVGLengthType unitType)
    : m_valueInSpecifiedUnits(0)
    , m_unit(storeUnit(mode, unitType))
{
    setValue(value, context);
}

ExceptionOr<void> SVGLength::setValueAsString(const String& valueAsString, SVGLengthMode mode)
{
    m_valueInSpecifiedUnits = 0;
    m_unit = storeUnit(mode, LengthTypeNumber);
    return setValueAsString(valueAsString);
}

bool SVGLength::operator==(const SVGLength& other) const
{
    return m_unit == other.m_unit
        && m_valueInSpecifiedUnits == other.m_valueInSpecifiedUnits;
}

bool SVGLength::operator!=(const SVGLength& other) const
{
    return !operator==(other);
}

SVGLength SVGLength::construct(SVGLengthMode mode, const String& valueAsString, SVGParsingError& parseError, SVGLengthNegativeValuesMode negativeValuesMode)
{
    SVGLength length(mode);

    if (length.setValueAsString(valueAsString).hasException())
        parseError = ParsingAttributeFailedError;
    else if (negativeValuesMode == ForbidNegativeLengths && length.valueInSpecifiedUnits() < 0)
        parseError = NegativeValueForbiddenError;

    return length;
}

SVGLengthType SVGLength::unitType() const
{
    return extractType(m_unit);
}

SVGLengthMode SVGLength::unitMode() const
{
    return extractMode(m_unit);
}

float SVGLength::value(const SVGLengthContext& context) const
{
    auto result = valueForBindings(context);
    if (result.hasException())
        return 0;
    return result.releaseReturnValue();
}

ExceptionOr<float> SVGLength::valueForBindings(const SVGLengthContext& context) const
{
    return context.convertValueToUserUnits(m_valueInSpecifiedUnits, extractMode(m_unit), extractType(m_unit));
}

ExceptionOr<void> SVGLength::setValue(const SVGLengthContext& context, float value, SVGLengthMode mode, SVGLengthType unitType)
{
    // FIXME: Seems like a bug that we change the value of m_unit even if setValue throws an exception.
    m_unit = storeUnit(mode, unitType);
    return setValue(value, context);
}

ExceptionOr<void> SVGLength::setValue(float value, const SVGLengthContext& context)
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (extractType(m_unit) == LengthTypePercentage)
        value = value / 100;

    auto convertedValue = context.convertValueFromUserUnits(value, extractMode(m_unit), extractType(m_unit));
    if (convertedValue.hasException())
        return convertedValue.releaseException();
    m_valueInSpecifiedUnits = convertedValue.releaseReturnValue();
    return { };
}
float SVGLength::valueAsPercentage() const
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (extractType(m_unit) == LengthTypePercentage)
        return m_valueInSpecifiedUnits / 100;

    return m_valueInSpecifiedUnits;
}

ExceptionOr<void> SVGLength::setValueAsString(const String& string)
{
    if (string.isEmpty())
        return { };

    float convertedNumber = 0;
    auto upconvertedCharacters = StringView(string).upconvertedCharacters();
    const UChar* ptr = upconvertedCharacters;
    const UChar* end = ptr + string.length();

    if (!parseNumber(ptr, end, convertedNumber, false))
        return Exception { SYNTAX_ERR };

    auto type = parseLengthType(ptr, end);
    if (type == LengthTypeUnknown)
        return Exception { SYNTAX_ERR };

    m_unit = storeUnit(extractMode(m_unit), type);
    m_valueInSpecifiedUnits = convertedNumber;
    return { };
}

String SVGLength::valueAsString() const
{
    return String::number(m_valueInSpecifiedUnits) + lengthTypeToString(extractType(m_unit));
}

ExceptionOr<void> SVGLength::newValueSpecifiedUnits(unsigned short type, float value)
{
    if (type == LengthTypeUnknown || type > LengthTypePC)
        return Exception { NOT_SUPPORTED_ERR };

    m_unit = storeUnit(extractMode(m_unit), static_cast<SVGLengthType>(type));
    m_valueInSpecifiedUnits = value;
    return { };
}

ExceptionOr<void> SVGLength::convertToSpecifiedUnits(unsigned short type, const SVGLengthContext& context)
{
    if (type == LengthTypeUnknown || type > LengthTypePC)
        return Exception { NOT_SUPPORTED_ERR };

    auto valueInUserUnits = valueForBindings(context);
    if (valueInUserUnits.hasException())
        return valueInUserUnits.releaseException();

    auto originalUnitAndType = m_unit;
    m_unit = storeUnit(extractMode(m_unit), static_cast<SVGLengthType>(type));
    auto result = setValue(valueInUserUnits.releaseReturnValue(), context);
    if (result.hasException()) {
        m_unit = originalUnitAndType;
        return result.releaseException();
    }

    return { };
}

SVGLength SVGLength::fromCSSPrimitiveValue(const CSSPrimitiveValue& value)
{
    SVGLengthType svgType;
    switch (value.primitiveType()) {
    case CSSPrimitiveValue::CSS_NUMBER:
        svgType = LengthTypeNumber;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        svgType = LengthTypePercentage;
        break;
    case CSSPrimitiveValue::CSS_EMS:
        svgType = LengthTypeEMS;
        break;
    case CSSPrimitiveValue::CSS_EXS:
        svgType = LengthTypeEXS;
        break;
    case CSSPrimitiveValue::CSS_PX:
        svgType = LengthTypePX;
        break;
    case CSSPrimitiveValue::CSS_CM:
        svgType = LengthTypeCM;
        break;
    case CSSPrimitiveValue::CSS_MM:
        svgType = LengthTypeMM;
        break;
    case CSSPrimitiveValue::CSS_IN:
        svgType = LengthTypeIN;
        break;
    case CSSPrimitiveValue::CSS_PT:
        svgType = LengthTypePT;
        break;
    case CSSPrimitiveValue::CSS_PC:
        svgType = LengthTypePC;
        break;
    case CSSPrimitiveValue::CSS_UNKNOWN:
    default:
        svgType = LengthTypeUnknown;
        break;
    };

    if (svgType == LengthTypeUnknown)
        return SVGLength();

    SVGLength length;
    length.newValueSpecifiedUnits(svgType, value.floatValue());
    return length;
}

Ref<CSSPrimitiveValue> SVGLength::toCSSPrimitiveValue(const SVGLength& length)
{
    CSSPrimitiveValue::UnitTypes cssType = CSSPrimitiveValue::CSS_UNKNOWN;
    switch (length.unitType()) {
    case LengthTypeUnknown:
        break;
    case LengthTypeNumber:
        cssType = CSSPrimitiveValue::CSS_NUMBER;
        break;
    case LengthTypePercentage:
        cssType = CSSPrimitiveValue::CSS_PERCENTAGE;
        break;
    case LengthTypeEMS:
        cssType = CSSPrimitiveValue::CSS_EMS;
        break;
    case LengthTypeEXS:
        cssType = CSSPrimitiveValue::CSS_EXS;
        break;
    case LengthTypePX:
        cssType = CSSPrimitiveValue::CSS_PX;
        break;
    case LengthTypeCM:
        cssType = CSSPrimitiveValue::CSS_CM;
        break;
    case LengthTypeMM:
        cssType = CSSPrimitiveValue::CSS_MM;
        break;
    case LengthTypeIN:
        cssType = CSSPrimitiveValue::CSS_IN;
        break;
    case LengthTypePT:
        cssType = CSSPrimitiveValue::CSS_PT;
        break;
    case LengthTypePC:
        cssType = CSSPrimitiveValue::CSS_PC;
        break;
    };

    return CSSPrimitiveValue::create(length.valueInSpecifiedUnits(), cssType);
}

SVGLengthMode SVGLength::lengthModeForAnimatedLengthAttribute(const QualifiedName& attrName)
{
    using Map = HashMap<QualifiedName, SVGLengthMode>;
    static NeverDestroyed<Map> map = [] {
        struct Mode {
            const QualifiedName& name;
            SVGLengthMode mode;
        };
        static const Mode modes[] = {
            { SVGNames::xAttr, LengthModeWidth },
            { SVGNames::yAttr, LengthModeHeight },
            { SVGNames::cxAttr, LengthModeWidth },
            { SVGNames::cyAttr, LengthModeHeight },
            { SVGNames::dxAttr, LengthModeWidth },
            { SVGNames::dyAttr, LengthModeHeight },
            { SVGNames::fxAttr, LengthModeWidth },
            { SVGNames::fyAttr, LengthModeHeight },
            { SVGNames::widthAttr, LengthModeWidth },
            { SVGNames::heightAttr, LengthModeHeight },
            { SVGNames::x1Attr, LengthModeWidth },
            { SVGNames::x2Attr, LengthModeWidth },
            { SVGNames::y1Attr, LengthModeHeight },
            { SVGNames::y2Attr, LengthModeHeight },
            { SVGNames::refXAttr, LengthModeWidth },
            { SVGNames::refYAttr, LengthModeHeight },
            { SVGNames::markerWidthAttr, LengthModeWidth },
            { SVGNames::markerHeightAttr, LengthModeHeight },
            { SVGNames::textLengthAttr, LengthModeWidth },
            { SVGNames::startOffsetAttr, LengthModeWidth },
        };
        Map map;
        for (auto& mode : modes)
            map.add(mode.name, mode.mode);
        return map;
    }();
    
    auto result = map.get().find(attrName);
    if (result == map.get().end())
        return LengthModeOther;
    return result->value;
}

TextStream& operator<<(TextStream& ts, const SVGLength& length)
{
    ts << length.valueAsString();
    return ts;
}

}
