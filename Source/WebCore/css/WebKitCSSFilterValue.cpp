/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitCSSFilterValue.h"

#include "CSSValueList.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

WebKitCSSFilterValue::WebKitCSSFilterValue(FilterOperationType operationType)
    : CSSValueList(WebKitCSSFilterClass, SpaceSeparator)
    , m_type(operationType)
{
}

String WebKitCSSFilterValue::customCSSText() const
{
    const char* result = "";
    switch (m_type) {
    case UnknownFilterOperation:
        result = "";
        break;
    case ReferenceFilterOperation:
        return CSSValueList::customCSSText();
    case GrayscaleFilterOperation:
        result = "grayscale(";
        break;
    case SepiaFilterOperation:
        result = "sepia(";
        break;
    case SaturateFilterOperation:
        result = "saturate(";
        break;
    case HueRotateFilterOperation:
        result = "hue-rotate(";
        break;
    case InvertFilterOperation:
        result = "invert(";
        break;
    case OpacityFilterOperation:
        result = "opacity(";
        break;
    case BrightnessFilterOperation:
        result = "brightness(";
        break;
    case ContrastFilterOperation:
        result = "contrast(";
        break;
    case BlurFilterOperation:
        result = "blur(";
        break;
    case DropShadowFilterOperation:
        result = "drop-shadow(";
        break;
    }

    return result + CSSValueList::customCSSText() + ')';
}

WebKitCSSFilterValue::WebKitCSSFilterValue(const WebKitCSSFilterValue& cloneFrom)
    : CSSValueList(cloneFrom)
    , m_type(cloneFrom.m_type)
{
}

Ref<WebKitCSSFilterValue> WebKitCSSFilterValue::cloneForCSSOM() const
{
    return adoptRef(*new WebKitCSSFilterValue(*this));
}

bool WebKitCSSFilterValue::equals(const WebKitCSSFilterValue& other) const
{
    return m_type == other.m_type && CSSValueList::equals(other);
}

}
