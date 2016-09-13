/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMathMLRoot.h"

#if ENABLE(MATHML)

#include "FontCache.h"
#include "GraphicsContext.h"
#include "MathMLNames.h"
#include "MathMLRowElement.h"
#include "PaintInfo.h"
#include "RenderIterator.h"
#include "RenderMathMLMenclose.h"
#include "RenderMathMLOperator.h"

static const UChar gRadicalCharacter = 0x221A;

namespace WebCore {

RenderMathMLRoot::RenderMathMLRoot(MathMLRowElement& element, RenderStyle&& style)
    : RenderMathMLRow(element, WTFMove(style))
{
    // Determine what kind of expression we have by element name
    if (element.hasTagName(MathMLNames::msqrtTag))
        m_kind = SquareRoot;
    else if (element.hasTagName(MathMLNames::mrootTag))
        m_kind = RootWithIndex;

    m_radicalOperator.setOperator(RenderMathMLRoot::style(), gRadicalCharacter, MathOperator::Type::VerticalOperator);
}

bool RenderMathMLRoot::isValid() const
{
    // Verify whether the list of children is valid:
    // <msqrt> child1 child2 ... childN </msqrt>
    // <mroot> base index </mroot>
    if (m_kind == SquareRoot)
        return true;

    ASSERT(m_kind == RootWithIndex);
    auto* child = firstChildBox();
    if (!child)
        return false;
    child = child->nextSiblingBox();
    return child && !child->nextSiblingBox();
}

RenderBox& RenderMathMLRoot::getBase() const
{
    ASSERT(isValid());
    ASSERT(m_kind == RootWithIndex);
    return *firstChildBox();
}

RenderBox& RenderMathMLRoot::getIndex() const
{
    ASSERT(isValid());
    ASSERT(m_kind == RootWithIndex);
    return *firstChildBox()->nextSiblingBox();
}

void RenderMathMLRoot::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLRow::styleDidChange(diff, oldStyle);
    m_radicalOperator.reset(style());
}

RenderMathMLRoot::HorizontalParameters RenderMathMLRoot::horizontalParameters()
{
    HorizontalParameters parameters;

    // Square roots do not require horizontal parameters.
    if (m_kind == SquareRoot)
        return parameters;

    // We try and read constants to draw the radical from the OpenType MATH and use fallback values otherwise.
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = style().fontCascade().primaryFont().mathData()) {
        parameters.kernBeforeDegree = mathData->getMathConstant(primaryFont, OpenTypeMathData::RadicalKernBeforeDegree);
        parameters.kernAfterDegree = mathData->getMathConstant(primaryFont, OpenTypeMathData::RadicalKernAfterDegree);
    } else {
        // RadicalKernBeforeDegree: No suggested value provided. OT Math Illuminated mentions 5/18 em, Gecko uses 0.
        // RadicalKernAfterDegree: Suggested value is -10/18 of em.
        parameters.kernBeforeDegree = 5 * style().fontCascade().size() / 18;
        parameters.kernAfterDegree = -10 * style().fontCascade().size() / 18;
    }
    return parameters;
}

RenderMathMLRoot::VerticalParameters RenderMathMLRoot::verticalParameters()
{
    VerticalParameters parameters;
    // We try and read constants to draw the radical from the OpenType MATH and use fallback values otherwise.
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = style().fontCascade().primaryFont().mathData()) {
        parameters.ruleThickness = mathData->getMathConstant(primaryFont, OpenTypeMathData::RadicalRuleThickness);
        parameters.verticalGap = mathData->getMathConstant(primaryFont, mathMLStyle()->displayStyle() ? OpenTypeMathData::RadicalDisplayStyleVerticalGap : OpenTypeMathData::RadicalVerticalGap);
        parameters.extraAscender = mathData->getMathConstant(primaryFont, OpenTypeMathData::RadicalExtraAscender);
        if (m_kind == RootWithIndex)
            parameters.degreeBottomRaisePercent = mathData->getMathConstant(primaryFont, OpenTypeMathData::RadicalDegreeBottomRaisePercent);
    } else {
        // RadicalVerticalGap: Suggested value is 5/4 default rule thickness.
        // RadicalDisplayStyleVerticalGap: Suggested value is default rule thickness + 1/4 x-height.
        // RadicalRuleThickness: Suggested value is default rule thickness.
        // RadicalExtraAscender: Suggested value is RadicalRuleThickness.
        // RadicalDegreeBottomRaisePercent: Suggested value is 60%.
        parameters.ruleThickness = ruleThicknessFallback();
        if (mathMLStyle()->displayStyle())
            parameters.verticalGap = parameters.ruleThickness + style().fontMetrics().xHeight() / 4;
        else
            parameters.verticalGap = 5 * parameters.ruleThickness / 4;

        if (m_kind == RootWithIndex) {
            parameters.extraAscender = parameters.ruleThickness;
            parameters.degreeBottomRaisePercent = 0.6f;
        }
    }
    return parameters;
}

void RenderMathMLRoot::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (!isValid()) {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;
        setPreferredLogicalWidthsDirty(false);
        return;
    }

    LayoutUnit preferredWidth = 0;
    if (m_kind == SquareRoot) {
        preferredWidth += m_radicalOperator.maxPreferredWidth();
        setPreferredLogicalWidthsDirty(true);
        RenderMathMLRow::computePreferredLogicalWidths();
        preferredWidth += m_maxPreferredLogicalWidth;
    } else {
        ASSERT(m_kind == RootWithIndex);
        auto horizontal = horizontalParameters();
        preferredWidth += horizontal.kernBeforeDegree;
        preferredWidth += getIndex().maxPreferredLogicalWidth();
        preferredWidth += horizontal.kernAfterDegree;
        preferredWidth += m_radicalOperator.maxPreferredWidth();
        preferredWidth += getBase().maxPreferredLogicalWidth();
    }

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredWidth;
    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLRoot::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    m_radicalOperatorTop = 0;
    m_baseWidth = 0;

    if (!isValid()) {
        setLogicalWidth(0);
        setLogicalHeight(0);
        clearNeedsLayout();
        return;
    }

    // We layout the children, determine the vertical metrics of the base and set the logical width.
    // Note: Per the MathML specification, the children of <msqrt> are wrapped in an inferred <mrow>, which is the desired base.
    LayoutUnit baseAscent, baseDescent;
    recomputeLogicalWidth();
    if (m_kind == SquareRoot) {
        baseAscent = baseDescent;
        RenderMathMLRow::computeLineVerticalStretch(baseAscent, baseDescent);
        RenderMathMLRow::layoutRowItems(baseAscent, baseDescent);
        m_baseWidth = logicalWidth();
    } else {
        getBase().layoutIfNeeded();
        m_baseWidth = getBase().logicalWidth();
        baseAscent = ascentForChild(getBase());
        baseDescent = getBase().logicalHeight() - baseAscent;
        getIndex().layoutIfNeeded();
    }

    auto horizontal = horizontalParameters();
    auto vertical = verticalParameters();

    // Stretch the radical operator to cover the base height.
    // We can then determine the metrics of the radical operator + the base.
    m_radicalOperator.stretchTo(style(), baseAscent + baseDescent);
    LayoutUnit radicalOperatorHeight = m_radicalOperator.ascent() + m_radicalOperator.descent();
    LayoutUnit indexBottomRaise = vertical.degreeBottomRaisePercent * radicalOperatorHeight;
    LayoutUnit radicalAscent = baseAscent + vertical.verticalGap + vertical.ruleThickness + vertical.extraAscender;
    LayoutUnit radicalDescent = std::max<LayoutUnit>(baseDescent, radicalOperatorHeight + vertical.extraAscender - radicalAscent);
    LayoutUnit descent = radicalDescent;
    LayoutUnit ascent = radicalAscent;

    // We set the logical width.
    if (m_kind == SquareRoot)
        setLogicalWidth(m_radicalOperator.width() + m_baseWidth);
    else {
        ASSERT(m_kind == RootWithIndex);
        setLogicalWidth(horizontal.kernBeforeDegree + getIndex().logicalWidth() + horizontal.kernAfterDegree + m_radicalOperator.width() + m_baseWidth);
    }

    // For <mroot>, we update the metrics to take into account the index.
    LayoutUnit indexAscent, indexDescent;
    if (m_kind == RootWithIndex) {
        indexAscent = ascentForChild(getIndex());
        indexDescent = getIndex().logicalHeight() - indexAscent;
        ascent = std::max<LayoutUnit>(radicalAscent, indexBottomRaise + indexDescent + indexAscent - descent);
    }

    // We set the final position of children.
    m_radicalOperatorTop = ascent - radicalAscent + vertical.extraAscender;
    LayoutUnit horizontalOffset = m_radicalOperator.width();
    if (m_kind == RootWithIndex)
        horizontalOffset += horizontal.kernBeforeDegree + getIndex().logicalWidth() + horizontal.kernAfterDegree;
    LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, m_baseWidth), ascent - baseAscent);
    if (m_kind == SquareRoot) {
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
            child->setLocation(child->location() + baseLocation);
    } else {
        ASSERT(m_kind == RootWithIndex);
        getBase().setLocation(baseLocation);
        LayoutPoint indexLocation(mirrorIfNeeded(horizontal.kernBeforeDegree, getIndex()), ascent + descent - indexBottomRaise - indexDescent - indexAscent);
        getIndex().setLocation(indexLocation);
    }

    setLogicalHeight(ascent + descent);
    clearNeedsLayout();
}

void RenderMathMLRoot::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLRow::paint(info, paintOffset);

    if (!firstChild() || info.context().paintingDisabled() || style().visibility() != VISIBLE || !isValid())
        return;

    // We draw the radical operator.
    LayoutPoint radicalOperatorTopLeft = paintOffset + location();
    LayoutUnit horizontalOffset = 0;
    if (m_kind == RootWithIndex) {
        auto horizontal = horizontalParameters();
        horizontalOffset = horizontal.kernBeforeDegree + getIndex().logicalWidth() + horizontal.kernAfterDegree;
    }
    radicalOperatorTopLeft.move(mirrorIfNeeded(horizontalOffset, m_radicalOperator.width()), m_radicalOperatorTop);
    m_radicalOperator.paint(style(), info, radicalOperatorTopLeft);

    // We draw the radical line.
    LayoutUnit ruleThickness = verticalParameters().ruleThickness;
    if (!ruleThickness)
        return;
    GraphicsContextStateSaver stateSaver(info.context());

    info.context().setStrokeThickness(ruleThickness);
    info.context().setStrokeStyle(SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColor(CSSPropertyColor));
    LayoutPoint ruleOffsetFrom = paintOffset + location() + LayoutPoint(0, m_radicalOperatorTop + ruleThickness / 2);
    LayoutPoint ruleOffsetTo = ruleOffsetFrom;
    horizontalOffset += m_radicalOperator.width();
    ruleOffsetFrom.move(mirrorIfNeeded(horizontalOffset), 0);
    horizontalOffset += m_baseWidth;
    ruleOffsetTo.move(mirrorIfNeeded(horizontalOffset), 0);
    info.context().drawLine(ruleOffsetFrom, ruleOffsetTo);
}

}

#endif // ENABLE(MATHML)
