/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "RenderRubyRun.h"

#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyText.h"
#include "RenderText.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/StackStats.h>

namespace WebCore {

using namespace std;

RenderRubyRun::RenderRubyRun(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
    , m_lastCharacter(0)
    , m_secondToLastCharacter(0)
{
    setReplaced(true);
    setInline(true);
}

RenderRubyRun::~RenderRubyRun()
{
}

bool RenderRubyRun::hasRubyText() const
{
    // The only place where a ruby text can be is in the first position
    // Note: As anonymous blocks, ruby runs do not have ':before' or ':after' content themselves.
    return firstChild() && firstChild()->isRubyText();
}

bool RenderRubyRun::hasRubyBase() const
{
    // The only place where a ruby base can be is in the last position
    // Note: As anonymous blocks, ruby runs do not have ':before' or ':after' content themselves.
    return lastChild() && lastChild()->isRubyBase();
}

RenderRubyText* RenderRubyRun::rubyText() const
{
    RenderObject* child = firstChild();
    // If in future it becomes necessary to support floating or positioned ruby text,
    // layout will have to be changed to handle them properly.
    ASSERT(!child || !child->isRubyText() || !child->isFloatingOrOutOfFlowPositioned());
    return child && child->isRubyText() ? static_cast<RenderRubyText*>(child) : 0;
}

RenderRubyBase* RenderRubyRun::rubyBase() const
{
    RenderObject* child = lastChild();
    return child && child->isRubyBase() ? static_cast<RenderRubyBase*>(child) : 0;
}

RenderRubyBase* RenderRubyRun::rubyBaseSafe()
{
    RenderRubyBase* base = rubyBase();
    if (!base) {
        base = createRubyBase();
        RenderBlockFlow::addChild(base);
    }
    return base;
}

RenderBlock* RenderRubyRun::firstLineBlock() const
{
    return 0;
}

void RenderRubyRun::updateFirstLetter()
{
}

bool RenderRubyRun::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isInline() || child.isRubyText();
}

void RenderRubyRun::addChild(RenderObject* child, RenderObject* beforeChild)
{
    ASSERT(child);

    if (child->isRubyText()) {
        if (!beforeChild) {
            // RenderRuby has already ascertained that we can add the child here.
            ASSERT(!hasRubyText());
            // prepend ruby texts as first child
            RenderBlockFlow::addChild(child, firstChild());
        }  else if (beforeChild->isRubyText()) {
            // New text is inserted just before another.
            // In this case the new text takes the place of the old one, and
            // the old text goes into a new run that is inserted as next sibling.
            ASSERT(beforeChild->parent() == this);
            RenderElement* ruby = parent();
            ASSERT(isRuby(ruby));
            RenderBlock* newRun = staticCreateRubyRun(ruby);
            ruby->addChild(newRun, nextSibling());
            // Add the new ruby text and move the old one to the new run
            // Note: Doing it in this order and not using RenderRubyRun's methods,
            // in order to avoid automatic removal of the ruby run in case there is no
            // other child besides the old ruby text.
            RenderBlockFlow::addChild(child, beforeChild);
            RenderBlockFlow::removeChild(*beforeChild);
            newRun->addChild(beforeChild);
        } else if (hasRubyBase()) {
            // Insertion before a ruby base object.
            // In this case we need insert a new run before the current one and split the base.
            RenderElement* ruby = parent();
            RenderRubyRun* newRun = staticCreateRubyRun(ruby);
            ruby->addChild(newRun, this);
            newRun->addChild(child);
            rubyBaseSafe()->moveChildren(newRun->rubyBaseSafe(), beforeChild);
        }
    } else {
        // child is not a text -> insert it into the base
        // (append it instead if beforeChild is the ruby text)
        if (beforeChild && beforeChild->isRubyText())
            beforeChild = 0;
        rubyBaseSafe()->addChild(child, beforeChild);
    }
}

void RenderRubyRun::removeChild(RenderObject& child)
{
    // If the child is a ruby text, then merge the ruby base with the base of
    // the right sibling run, if possible.
    if (!beingDestroyed() && !documentBeingDestroyed() && child.isRubyText()) {
        RenderRubyBase* base = rubyBase();
        RenderObject* rightNeighbour = nextSibling();
        if (base && is<RenderRubyRun>(rightNeighbour)) {
            // Ruby run without a base can happen only at the first run.
            RenderRubyRun& rightRun = downcast<RenderRubyRun>(*rightNeighbour);
            if (rightRun.hasRubyBase()) {
                RenderRubyBase* rightBase = rightRun.rubyBaseSafe();
                // Collect all children in a single base, then swap the bases.
                rightBase->mergeChildrenWithBase(base);
                moveChildTo(&rightRun, base);
                rightRun.moveChildTo(this, rightBase);
                // The now empty ruby base will be removed below.
                ASSERT(!rubyBase()->firstChild());
            }
        }
    }

    RenderBlockFlow::removeChild(child);

    if (!beingDestroyed() && !documentBeingDestroyed()) {
        // Check if our base (if any) is now empty. If so, destroy it.
        RenderBlock* base = rubyBase();
        if (base && !base->firstChild()) {
            RenderBlockFlow::removeChild(*base);
            base->deleteLines();
            base->destroy();
        }

        // If any of the above leaves the run empty, destroy it as well.
        if (!hasRubyText() && !hasRubyBase()) {
            parent()->removeChild(*this);
            deleteLines();
            destroy();
        }
    }
}

RenderRubyBase* RenderRubyRun::createRubyBase() const
{
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK);
    newStyle.setTextAlign(CENTER); // FIXME: use WEBKIT_CENTER?
    auto renderer = new RenderRubyBase(document(), WTFMove(newStyle));
    renderer->initializeStyle();
    return renderer;
}

RenderRubyRun* RenderRubyRun::staticCreateRubyRun(const RenderObject* parentRuby)
{
    ASSERT(isRuby(parentRuby));
    auto renderer = new RenderRubyRun(parentRuby->document(), RenderStyle::createAnonymousStyleWithDisplay(parentRuby->style(), INLINE_BLOCK));
    renderer->initializeStyle();
    return renderer;
}

RenderObject* RenderRubyRun::layoutSpecialExcludedChild(bool relayoutChildren)
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    // Don't bother positioning the RenderRubyRun yet.
    RenderRubyText* rt = rubyText();
    if (!rt)
        return 0;
    if (relayoutChildren)
        rt->setChildNeedsLayout(MarkOnlyThis);
    rt->layoutIfNeeded();
    return rt;
}

void RenderRubyRun::layout()
{
    if (RenderRubyBase* base = rubyBase())
        base->reset();

    RenderBlockFlow::layout();
    
    RenderRubyText* rt = rubyText();
    if (!rt)
        return;

    rt->setLogicalLeft(0);
    
    // Place the RenderRubyText such that its bottom is flush with the lineTop of the first line of the RenderRubyBase.
    LayoutUnit lastLineRubyTextBottom = rt->logicalHeight();
    LayoutUnit firstLineRubyTextTop = 0;
    RootInlineBox* rootBox = rt->lastRootBox();
    if (rootBox) {
        // In order to align, we have to ignore negative leading.
        firstLineRubyTextTop = rt->firstRootBox()->logicalTopLayoutOverflow();
        lastLineRubyTextBottom = rootBox->logicalBottomLayoutOverflow();
    }
    
    if (isHorizontalWritingMode() && rt->style().rubyPosition() == RubyPositionInterCharacter) {
        // Bopomofo. We need to move the RenderRubyText over to the right side and center it
        // vertically relative to the base.
        const FontCascade& font = style().fontCascade();
        float distanceBetweenBase = max(font.letterSpacing(), 2.0f * rt->style().fontCascade().fontMetrics().height());
        setWidth(width() + distanceBetweenBase - font.letterSpacing());
        if (RenderRubyBase* rb = rubyBase()) {
            LayoutUnit firstLineTop = 0;
            LayoutUnit lastLineBottom = logicalHeight();
            RootInlineBox* rootBox = rb->firstRootBox();
            if (rootBox)
                firstLineTop = rootBox->logicalTopLayoutOverflow();
            firstLineTop += rb->logicalTop();
            if (rootBox)
                lastLineBottom = rootBox->logicalBottomLayoutOverflow();
            lastLineBottom += rb->logicalTop();
            rt->setX(rb->x() + rb->width() - font.letterSpacing());
            LayoutUnit extent = lastLineBottom - firstLineTop;
            rt->setY(firstLineTop + (extent - rt->height()) / 2);
        }
    } else if (style().isFlippedLinesWritingMode() == (style().rubyPosition() == RubyPositionAfter)) {
        LayoutUnit firstLineTop = 0;
        if (RenderRubyBase* rb = rubyBase()) {
            RootInlineBox* rootBox = rb->firstRootBox();
            if (rootBox)
                firstLineTop = rootBox->logicalTopLayoutOverflow();
            firstLineTop += rb->logicalTop();
        }
        
        rt->setLogicalTop(-lastLineRubyTextBottom + firstLineTop);
    } else {
        LayoutUnit lastLineBottom = logicalHeight();
        if (RenderRubyBase* rb = rubyBase()) {
            RootInlineBox* rootBox = rb->lastRootBox();
            if (rootBox)
                lastLineBottom = rootBox->logicalBottomLayoutOverflow();
            lastLineBottom += rb->logicalTop();
        }

        rt->setLogicalTop(-firstLineRubyTextTop + lastLineBottom);
    }

    // Update our overflow to account for the new RenderRubyText position.
    computeOverflow(clientLogicalBottom());
}

static bool shouldOverhang(bool firstLine, const RenderObject* renderer, const RenderRubyBase& rubyBase)
{
    if (!renderer || !renderer->isText())
        return false;
    const RenderStyle& rubyBaseStyle = firstLine ? rubyBase.firstLineStyle() : rubyBase.style();
    const RenderStyle& style = firstLine ? renderer->firstLineStyle() : renderer->style();
    return style.fontSize() <= rubyBaseStyle.fontSize();
}

void RenderRubyRun::getOverhang(bool firstLine, RenderObject* startRenderer, RenderObject* endRenderer, float& startOverhang, float& endOverhang) const
{
    ASSERT(!needsLayout());

    startOverhang = 0;
    endOverhang = 0;

    RenderRubyBase* rubyBase = this->rubyBase();
    RenderRubyText* rubyText = this->rubyText();

    if (!rubyBase || !rubyText)
        return;

    if (!rubyBase->firstRootBox())
        return;

    LayoutUnit logicalWidth = this->logicalWidth();
    float logicalLeftOverhang = std::numeric_limits<float>::max();
    float logicalRightOverhang = std::numeric_limits<float>::max();
    for (RootInlineBox* rootInlineBox = rubyBase->firstRootBox(); rootInlineBox; rootInlineBox = rootInlineBox->nextRootBox()) {
        logicalLeftOverhang = std::min<float>(logicalLeftOverhang, rootInlineBox->logicalLeft());
        logicalRightOverhang = std::min<float>(logicalRightOverhang, logicalWidth - rootInlineBox->logicalRight());
    }

    startOverhang = style().isLeftToRightDirection() ? logicalLeftOverhang : logicalRightOverhang;
    endOverhang = style().isLeftToRightDirection() ? logicalRightOverhang : logicalLeftOverhang;

    if (!shouldOverhang(firstLine, startRenderer, *rubyBase))
        startOverhang = 0;
    if (!shouldOverhang(firstLine, endRenderer, *rubyBase))
        endOverhang = 0;

    // We overhang a ruby only if the neighboring render object is a text.
    // We can overhang the ruby by no more than half the width of the neighboring text
    // and no more than half the font size.
    const RenderStyle& rubyTextStyle = firstLine ? rubyText->firstLineStyle() : rubyText->style();
    float halfWidthOfFontSize = rubyTextStyle.fontSize() / 2.;
    if (startOverhang)
        startOverhang = std::min(startOverhang, std::min(downcast<RenderText>(*startRenderer).minLogicalWidth(), halfWidthOfFontSize));
    if (endOverhang)
        endOverhang = std::min(endOverhang, std::min(downcast<RenderText>(*endRenderer).minLogicalWidth(), halfWidthOfFontSize));
}

void RenderRubyRun::updatePriorContextFromCachedBreakIterator(LazyLineBreakIterator& iterator) const
{
    iterator.setPriorContext(m_lastCharacter, m_secondToLastCharacter);
}

bool RenderRubyRun::canBreakBefore(const LazyLineBreakIterator& iterator) const
{
    RenderRubyText* rubyText = this->rubyText();
    if (!rubyText)
        return true;
    return rubyText->canBreakBefore(iterator);
}

} // namespace WebCore
