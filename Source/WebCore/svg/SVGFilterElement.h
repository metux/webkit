/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#pragma once

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedLength.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGFilterElement final : public SVGElement,
                               public SVGURIReference,
                               public SVGExternalResourcesRequired {
public:
    static Ref<SVGFilterElement> create(const QualifiedName&, Document&);

    void setFilterRes(unsigned filterResX, unsigned filterResY);

private:
    SVGFilterElement(const QualifiedName&, Document&);

    bool needsPendingResourceHandling() const final { return false; }

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    void childrenChanged(const ChildChange&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;

    bool selfHasRelativeLengths() const final { return true; }

    static const AtomicString& filterResXIdentifier();
    static const AtomicString& filterResYIdentifier();

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFilterElement)
        DECLARE_ANIMATED_ENUMERATION(FilterUnits, filterUnits, SVGUnitTypes::SVGUnitType)
        DECLARE_ANIMATED_ENUMERATION(PrimitiveUnits, primitiveUnits, SVGUnitTypes::SVGUnitType)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_INTEGER(FilterResX, filterResX)
        DECLARE_ANIMATED_INTEGER(FilterResY, filterResY)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore
