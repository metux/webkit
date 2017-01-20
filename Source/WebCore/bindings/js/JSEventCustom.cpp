/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "JSEvent.h"

#include "DataTransfer.h"
#include "Event.h"
#include "EventHeaders.h"
#include "EventInterfaces.h"
#include "JSDOMBinding.h"
#include "JSDataTransfer.h"
#include <runtime/JSLock.h>
#include <wtf/text/AtomicString.h>

using namespace JSC;

namespace WebCore {

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    case interfaceName##InterfaceType: \
        return createWrapper<interfaceName>(globalObject, WTFMove(event));

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<Event>&& event)
{
    switch (event->eventInterface()) {
        DOM_EVENT_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE)
    }

    return createWrapper<Event>(globalObject, WTFMove(event));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, Event& event)
{
    return wrap(state, globalObject, event);
}

#undef TRY_TO_WRAP_WITH_INTERFACE

} // namespace WebCore
