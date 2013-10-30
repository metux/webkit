/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "Blob.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "InspectorInstrumentation.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "XMLHttpRequest.h"
#include <interpreter/StackIterator.h>
#include <runtime/ArrayBuffer.h>
#include <runtime/Error.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSArrayBufferView.h>

using namespace JSC;

namespace WebCore {

void JSXMLHttpRequest::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSXMLHttpRequest* thisObject = jsCast<JSXMLHttpRequest*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);

    if (XMLHttpRequestUpload* upload = thisObject->m_impl->optionalUpload())
        visitor.addOpaqueRoot(upload);

    if (Document* responseDocument = thisObject->m_impl->optionalResponseXML())
        visitor.addOpaqueRoot(responseDocument);

    if (ArrayBuffer* responseArrayBuffer = thisObject->m_impl->optionalResponseArrayBuffer())
        visitor.addOpaqueRoot(responseArrayBuffer);

    if (Blob* responseBlob = thisObject->m_impl->optionalResponseBlob())
        visitor.addOpaqueRoot(responseBlob);

    thisObject->m_impl->visitJSEventListeners(visitor);
}

// Custom functions
JSValue JSXMLHttpRequest::open(ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    const KURL& url = impl()->scriptExecutionContext()->completeURL(exec->argument(1).toString(exec)->value(exec));
    String method = exec->argument(0).toString(exec)->value(exec);

    ExceptionCode ec = 0;
    if (exec->argumentCount() >= 3) {
        bool async = exec->argument(2).toBoolean(exec);

        if (exec->argumentCount() >= 4 && !exec->argument(3).isUndefined()) {
            String user = valueToStringWithNullCheck(exec, exec->argument(3));

            if (exec->argumentCount() >= 5 && !exec->argument(4).isUndefined()) {
                String password = valueToStringWithNullCheck(exec, exec->argument(4));
                impl()->open(method, url, async, user, password, ec);
            } else
                impl()->open(method, url, async, user, ec);
        } else
            impl()->open(method, url, async, ec);
    } else
        impl()->open(method, url, ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSXMLHttpRequest::send(ExecState* exec)
{
    InspectorInstrumentation::willSendXMLHttpRequest(impl()->scriptExecutionContext(), impl()->url());

    ExceptionCode ec = 0;
    if (!exec->argumentCount())
        impl()->send(ec);
    else {
        JSValue val = exec->argument(0);
        if (val.isUndefinedOrNull())
            impl()->send(ec);
        else if (val.inherits(JSDocument::info()))
            impl()->send(toDocument(val), ec);
        else if (val.inherits(JSBlob::info()))
            impl()->send(toBlob(val), ec);
        else if (val.inherits(JSDOMFormData::info()))
            impl()->send(toDOMFormData(val), ec);
        else if (val.inherits(JSArrayBuffer::info()))
            impl()->send(toArrayBuffer(val), ec);
        else if (val.inherits(JSArrayBufferView::info())) {
            RefPtr<ArrayBufferView> view = toArrayBufferView(val);
            impl()->send(view.get(), ec);
        } else
            impl()->send(val.toString(exec)->value(exec), ec);
    }

    StackIterator iter = exec->begin();
    ++iter;
    if (iter != exec->end()) {
        unsigned line = 0;
        unsigned unusuedColumn = 0;
        iter->computeLineAndColumn(line, unusuedColumn);
        impl()->setLastSendLineNumber(line);
        impl()->setLastSendURL(iter->sourceURL());
    } else {
        impl()->setLastSendLineNumber(0);
        impl()->setLastSendURL(String());
    }
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSXMLHttpRequest::responseText(ExecState* exec) const
{
    ExceptionCode ec = 0;
    String text = impl()->responseText(ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return jsOwnedStringOrNull(exec, text);
}

JSValue JSXMLHttpRequest::response(ExecState* exec) const
{
    switch (impl()->responseTypeCode()) {
    case XMLHttpRequest::ResponseTypeDefault:
    case XMLHttpRequest::ResponseTypeText:
        return responseText(exec);

    case XMLHttpRequest::ResponseTypeDocument:
        {
            ExceptionCode ec = 0;
            Document* document = impl()->responseXML(ec);
            if (ec) {
                setDOMException(exec, ec);
                return jsUndefined();
            }
            return toJS(exec, globalObject(), document);
        }

    case XMLHttpRequest::ResponseTypeBlob:
        {
            ExceptionCode ec = 0;
            Blob* blob = impl()->responseBlob(ec);
            if (ec) {
                setDOMException(exec, ec);
                return jsUndefined();
            }
            return toJS(exec, globalObject(), blob);
        }

    case XMLHttpRequest::ResponseTypeArrayBuffer:
        {
            ExceptionCode ec = 0;
            ArrayBuffer* arrayBuffer = impl()->responseArrayBuffer(ec);
            if (ec) {
                setDOMException(exec, ec);
                return jsUndefined();
            }
            return toJS(exec, globalObject(), arrayBuffer);
        }
    }

    return jsUndefined();
}

} // namespace WebCore
