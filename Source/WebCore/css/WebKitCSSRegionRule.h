/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef WebKitCSSRegionRule_h
#define WebKitCSSRegionRule_h

#include "CSSGroupingRule.h"

#if ENABLE(CSS_REGIONS)

namespace WebCore {

class StyleRuleRegion;

class WebKitCSSRegionRule : public CSSGroupingRule {
public:
    static PassRefPtr<WebKitCSSRegionRule> create(StyleRuleRegion* rule, CSSStyleSheet* sheet) { return adoptRef(new WebKitCSSRegionRule(rule, sheet)); }

    virtual CSSRule::Type type() const override { return WEBKIT_REGION_RULE; }
    virtual String cssText() const override;

private:
    WebKitCSSRegionRule(StyleRuleRegion*, CSSStyleSheet* parent);
};

CSS_RULE_TYPE_CASTS(WebKitCSSRegionRule, CSSRule::WEBKIT_REGION_RULE)

}

#endif // ENABLE(CSS_REGIONS)

#endif // WebKitCSSRegionRule_h
