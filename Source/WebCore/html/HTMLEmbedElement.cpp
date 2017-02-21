/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "HTMLEmbedElement.h"

#include "CSSPropertyNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "PluginDocument.h"
#include "RenderEmbeddedObject.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLPlugInImageElement(tagName, document, createdByParser)
{
    ASSERT(hasTagName(embedTag));
}

Ref<HTMLEmbedElement> HTMLEmbedElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(*new HTMLEmbedElement(tagName, document, createdByParser));
}

Ref<HTMLEmbedElement> HTMLEmbedElement::create(Document& document)
{
    return adoptRef(*new HTMLEmbedElement(embedTag, document, false));
}

static inline RenderWidget* findWidgetRenderer(const Node* node)
{
    if (!node->renderer()) {
        do {
            node = node->parentNode();
        } while (node && !is<HTMLObjectElement>(*node));
    }

    if (node && is<RenderWidget>(node->renderer()))
        return downcast<RenderWidget>(node->renderer());

    return nullptr;
}

RenderWidget* HTMLEmbedElement::renderWidgetLoadingPlugin() const
{
    FrameView* view = document().view();
    if (!view || (!view->isInRenderTreeLayout() && !view->isPainting())) {
        // Needs to load the plugin immediatedly because this function is called
        // when JavaScript code accesses the plugin.
        // FIXME: <rdar://16893708> Check if dispatching events here is safe.
        document().updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
    }
    return findWidgetRenderer(this);
}

bool HTMLEmbedElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == hiddenAttr)
        return true;
    return HTMLPlugInImageElement::isPresentationAttribute(name);
}

void HTMLEmbedElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == hiddenAttr) {
        if (equalLettersIgnoringASCIICase(value, "yes") || equalLettersIgnoringASCIICase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, 0, CSSPrimitiveValue::CSS_PX);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else
        HTMLPlugInImageElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLEmbedElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == typeAttr) {
        m_serviceType = value.string().left(value.find(';')).convertToASCIILowercase();
        // FIXME: The only difference between this and HTMLObjectElement's corresponding
        // code is that HTMLObjectElement does setNeedsWidgetUpdate(true). Consider moving
        // this up to the HTMLPlugInImageElement to be shared.
    } else if (name == codeAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        // FIXME: Why no call to the image loader?
        // FIXME: If both code and src attributes are specified, last one parsed/changed wins. That can't be right!
    } else if (name == srcAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        document().updateStyleIfNeeded();
        if (renderer() && isImageType()) {
            if (!m_imageLoader)
                m_imageLoader = std::make_unique<HTMLImageLoader>(*this);
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
        // FIXME: If both code and src attributes are specified, last one parsed/changed wins. That can't be right!
    } else
        HTMLPlugInImageElement::parseAttribute(name, value);
}

void HTMLEmbedElement::parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues)
{
    if (!hasAttributes())
        return;

    for (const Attribute& attribute : attributesIterator()) {
        paramNames.append(attribute.localName().string());
        paramValues.append(attribute.value().string());
    }
}

// FIXME: This should be unified with HTMLObjectElement::updateWidget and
// moved down into HTMLPluginImageElement.cpp
void HTMLEmbedElement::updateWidget(CreatePlugins createPlugins)
{
    ASSERT(!renderEmbeddedObject()->isPluginUnavailable());
    ASSERT(needsWidgetUpdate());
    setNeedsWidgetUpdate(false);

    if (m_url.isEmpty() && m_serviceType.isEmpty())
        return;

    // Note these pass m_url and m_serviceType to allow better code sharing with
    // <object> which modifies url and serviceType before calling these.
    if (!allowedToLoadFrameURL(m_url))
        return;

    // FIXME: It's sadness that we have this special case here.
    //        See http://trac.webkit.org/changeset/25128 and
    //        plugins/netscape-plugin-setwindow-size.html
    if (createPlugins == CreatePlugins::No && wouldLoadAsPlugIn(m_url, m_serviceType)) {
        // Ensure updateWidget() is called again during layout to create the Netscape plug-in.
        setNeedsWidgetUpdate(true);
        return;
    }

    // FIXME: These should be joined into a PluginParameters class.
    Vector<String> paramNames;
    Vector<String> paramValues;
    parametersForPlugin(paramNames, paramValues);

    Ref<HTMLEmbedElement> protectedThis(*this); // Loading the plugin might remove us from the document.
    bool beforeLoadAllowedLoad = guardedDispatchBeforeLoadEvent(m_url);
    if (!beforeLoadAllowedLoad) {
        if (is<PluginDocument>(document())) {
            // Plugins inside plugin documents load differently than other plugins. By the time
            // we are here in a plugin document, the load of the plugin (which is the plugin document's
            // main resource) has already started. We need to explicitly cancel the main resource load here.
            downcast<PluginDocument>(document()).cancelManualPluginLoad();
        }
        return;
    }
    if (!renderer()) // Do not load the plugin if beforeload removed this element or its renderer.
        return;

    // beforeLoad could have changed the document. Make sure the URL is still safe to load.
    if (!allowedToLoadFrameURL(m_url))
        return;

    // FIXME: beforeLoad could have detached the renderer!  Just like in the <object> case above.
    requestObject(m_url, m_serviceType, paramNames, paramValues);
}

bool HTMLEmbedElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!hasAttributeWithoutSynchronization(typeAttr) && !hasAttributeWithoutSynchronization(srcAttr))
        return false;

    if (isImageType())
        return HTMLPlugInImageElement::rendererIsNeeded(style);

    // If my parent is an <object> and is not set to use fallback content, I
    // should be ignored and not get a renderer.
    ContainerNode* parent = parentNode();
    if (is<HTMLObjectElement>(parent)) {
        if (!parent->renderer())
            return false;
        if (!downcast<HTMLObjectElement>(*parent).useFallbackContent()) {
            ASSERT(!parent->renderer()->isEmbeddedObject());
            return false;
        }
    }

#if ENABLE(DASHBOARD_SUPPORT)
    // Workaround for <rdar://problem/6642221>.
    if (document().settings().usesDashboardBackwardCompatibilityMode())
        return true;
#endif

    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

bool HTMLEmbedElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLPlugInImageElement::isURLAttribute(attribute);
}

const AtomicString& HTMLEmbedElement::imageSourceURL() const
{
    return attributeWithoutSynchronization(srcAttr);
}

void HTMLEmbedElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(srcAttr)));
}

}
