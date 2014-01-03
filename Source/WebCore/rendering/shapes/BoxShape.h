/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BoxShape_h
#define BoxShape_h

#include "FloatRoundedRect.h"
#include "Shape.h"

namespace WebCore {

class BoxShape : public Shape {
public:
    BoxShape(const FloatRoundedRect& bounds, float shapeMargin, float shapePadding);

    virtual LayoutRect shapeMarginLogicalBoundingBox() const OVERRIDE { return static_cast<LayoutRect>(m_marginBounds.rect()); }
    virtual LayoutRect shapePaddingLogicalBoundingBox() const OVERRIDE { return static_cast<LayoutRect>(m_paddingBounds.rect()); }
    virtual bool isEmpty() const OVERRIDE { return m_bounds.isEmpty(); }
    virtual void getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList&) const OVERRIDE;
    virtual void getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList&) const OVERRIDE;
    virtual bool firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const LayoutSize& minLogicalIntervalSize, LayoutUnit&) const OVERRIDE;

    virtual void buildDisplayPaths(DisplayPaths&) const OVERRIDE;

private:
    FloatRoundedRect m_bounds;
    FloatRoundedRect m_marginBounds;
    FloatRoundedRect m_paddingBounds;
};

} // namespace WebCore

#endif // BoxShape_h