/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef RenderTextFragment_h
#define RenderTextFragment_h

#include "RenderText.h"

namespace WebCore {

// Used to represent a text substring of an element, e.g., for text runs that are split because of
// first letter and that must therefore have different styles (and positions in the render tree).
// We cache offsets so that text transformations can be applied in such a way that we can recover
// the original unaltered string from our corresponding DOM node.
class RenderTextFragment FINAL : public RenderText {
public:
    RenderTextFragment(Text&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&);

    virtual ~RenderTextFragment();

    virtual bool isTextFragment() const OVERRIDE { return true; }

    virtual bool canBeSelectionLeaf() const OVERRIDE;

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    RenderBoxModelObject* firstLetter() const { return m_firstLetter; }
    void setFirstLetter(RenderBoxModelObject& firstLetter) { m_firstLetter = &firstLetter; }

    StringImpl* contentString() const { return m_contentString.impl(); }

    virtual void setText(const String&, bool force = false) OVERRIDE;

    const String& altText() const { return m_altText; }
    void setAltText(const String& altText) { m_altText = altText; }
    
private:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;
    virtual void willBeDestroyed() OVERRIDE;

    virtual UChar previousCharacter() const OVERRIDE;
    RenderBlock* blockForAccompanyingFirstLetter();

    unsigned m_start;
    unsigned m_end;
    // Alternative description that can be used for accessibility instead of the native text.
    String m_altText;
    String m_contentString;
    RenderBoxModelObject* m_firstLetter;
};

inline RenderTextFragment* toRenderTextFragment(RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || toRenderText(object)->isTextFragment());
    return static_cast<RenderTextFragment*>(object);
}

inline const RenderTextFragment* toRenderTextFragment(const RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || toRenderText(object)->isTextFragment());
    return static_cast<const RenderTextFragment*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderTextFragment(const RenderTextFragment*);

} // namespace WebCore

#endif // RenderTextFragment_h
