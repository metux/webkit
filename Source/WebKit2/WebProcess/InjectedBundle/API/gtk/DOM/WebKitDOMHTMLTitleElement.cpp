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
#include "WebKitDOMHTMLTitleElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/ExceptionCodeDescription.h>
#include "GObjectEventListener.h"
#include <WebCore/JSMainThreadExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLTitleElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

WebKitDOMHTMLTitleElement* kit(WebCore::HTMLTitleElement* obj)
{
    return WEBKIT_DOM_HTML_TITLE_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLTitleElement* core(WebKitDOMHTMLTitleElement* request)
{
    return request ? static_cast<WebCore::HTMLTitleElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLTitleElement* wrapHTMLTitleElement(WebCore::HTMLTitleElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_TITLE_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_TITLE_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_title_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLTitleElement* coreTarget = static_cast<WebCore::HTMLTitleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    WebCore::ExceptionCode ec = 0;
    gboolean result = coreTarget->dispatchEventForBindings(*coreEvent, ec);
    if (ec) {
        WebCore::ExceptionCodeDescription description(ec);
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.code, description.name);
    }
    return result;
}

static gboolean webkit_dom_html_title_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTitleElement* coreTarget = static_cast<WebCore::HTMLTitleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_title_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLTitleElement* coreTarget = static_cast<WebCore::HTMLTitleElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_title_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_title_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_title_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLTitleElement, webkit_dom_html_title_element, WEBKIT_DOM_TYPE_HTML_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_event_target_init))

enum {
    PROP_0,
    PROP_TEXT,
};

static void webkit_dom_html_title_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTitleElement* self = WEBKIT_DOM_HTML_TITLE_ELEMENT(object);

    switch (propertyId) {
    case PROP_TEXT:
        webkit_dom_html_title_element_set_text(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_title_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLTitleElement* self = WEBKIT_DOM_HTML_TITLE_ELEMENT(object);

    switch (propertyId) {
    case PROP_TEXT:
        g_value_take_string(value, webkit_dom_html_title_element_get_text(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_title_element_class_init(WebKitDOMHTMLTitleElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_title_element_set_property;
    gobjectClass->get_property = webkit_dom_html_title_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        PROP_TEXT,
        g_param_spec_string(
            "text",
            "HTMLTitleElement:text",
            "read-write gchar* HTMLTitleElement:text",
            "",
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_title_element_init(WebKitDOMHTMLTitleElement* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_html_title_element_get_text(WebKitDOMHTMLTitleElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TITLE_ELEMENT(self), 0);
    WebCore::HTMLTitleElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->text());
    return result;
}

void webkit_dom_html_title_element_set_text(WebKitDOMHTMLTitleElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_TITLE_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLTitleElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setText(convertedValue);
}

