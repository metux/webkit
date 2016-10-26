/*
 *  This file is part of the WebKit open source project.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitDOMHTMLPreElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/ExceptionCodeDescription.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSMainThreadExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLPreElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

WebKitDOMHTMLPreElement* kit(WebCore::HTMLPreElement* obj)
{
    return WEBKIT_DOM_HTML_PRE_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLPreElement* core(WebKitDOMHTMLPreElement* request)
{
    return request ? static_cast<WebCore::HTMLPreElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLPreElement* wrapHTMLPreElement(WebCore::HTMLPreElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_PRE_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_PRE_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_pre_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLPreElement* coreTarget = static_cast<WebCore::HTMLPreElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    WebCore::ExceptionCode ec = 0;
    gboolean result = coreTarget->dispatchEventForBindings(*coreEvent, ec);
    if (ec) {
        WebCore::ExceptionCodeDescription description(ec);
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.code, description.name);
    }
    return result;
}

static gboolean webkit_dom_html_pre_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLPreElement* coreTarget = static_cast<WebCore::HTMLPreElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_pre_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLPreElement* coreTarget = static_cast<WebCore::HTMLPreElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_pre_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_pre_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_pre_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLPreElement, webkit_dom_html_pre_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_event_target_init))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_WRAP,
};

static void webkit_dom_html_pre_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLPreElement* self = WEBKIT_DOM_HTML_PRE_ELEMENT(object);

    switch (propertyId) {
    case PROP_WIDTH:
        webkit_dom_html_pre_element_set_width(self, g_value_get_long(value));
        break;
    case PROP_WRAP:
        webkit_dom_html_pre_element_set_wrap(self, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_pre_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLPreElement* self = WEBKIT_DOM_HTML_PRE_ELEMENT(object);

    switch (propertyId) {
    case PROP_WIDTH:
        g_value_set_long(value, webkit_dom_html_pre_element_get_width(self));
        break;
    case PROP_WRAP:
        g_value_set_boolean(value, webkit_dom_html_pre_element_get_wrap(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_pre_element_class_init(WebKitDOMHTMLPreElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_pre_element_set_property;
    gobjectClass->get_property = webkit_dom_html_pre_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        PROP_WIDTH,
        g_param_spec_long(
            "width",
            "HTMLPreElement:width",
            "read-write glong HTMLPreElement:width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        PROP_WRAP,
        g_param_spec_boolean(
            "wrap",
            "HTMLPreElement:wrap",
            "read-write gboolean HTMLPreElement:wrap",
            FALSE,
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_pre_element_init(WebKitDOMHTMLPreElement* request)
{
    UNUSED_PARAM(request);
}

glong webkit_dom_html_pre_element_get_width(WebKitDOMHTMLPreElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_PRE_ELEMENT(self), 0);
    WebCore::HTMLPreElement* item = WebKit::core(self);
    glong result = item->getIntegralAttribute(WebCore::HTMLNames::widthAttr);
    return result;
}

void webkit_dom_html_pre_element_set_width(WebKitDOMHTMLPreElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_PRE_ELEMENT(self));
    WebCore::HTMLPreElement* item = WebKit::core(self);
    item->setIntegralAttribute(WebCore::HTMLNames::widthAttr, value);
}

gboolean webkit_dom_html_pre_element_get_wrap(WebKitDOMHTMLPreElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_PRE_ELEMENT(self), FALSE);
    WebCore::HTMLPreElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::wrapAttr);
    return result;
}

void webkit_dom_html_pre_element_set_wrap(WebKitDOMHTMLPreElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_PRE_ELEMENT(self));
    WebCore::HTMLPreElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::wrapAttr, value);
}

