/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InspectorAgent.h"

#include "InstrumentingAgents.h"
#include "Page.h"
#include "Settings.h"
#include <bindings/ScriptValue.h>
#include <inspector/InspectorJSFrontendDispatchers.h>
#include <inspector/InspectorValues.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

using namespace Inspector;

namespace WebCore {

InspectorAgent::InspectorAgent(Page* page, InstrumentingAgents* instrumentingAgents)
    : InspectorAgentBase(ASCIILiteral("Inspector"), instrumentingAgents)
    , m_inspectedPage(page)
    , m_enabled(false)
{
    ASSERT_ARG(page, page);
    m_instrumentingAgents->setInspectorAgent(this);
}

InspectorAgent::~InspectorAgent()
{
    m_instrumentingAgents->setInspectorAgent(0);
}

void InspectorAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorInspectorFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorInspectorBackendDispatcher::create(backendDispatcher, this);
}

void InspectorAgent::willDestroyFrontendAndBackend()
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    m_pendingEvaluateTestCommands.clear();

    ErrorString error;
    disable(&error);
}

void InspectorAgent::enable(ErrorString*)
{
    m_enabled = true;

    if (m_pendingInspectData.first)
        inspect(m_pendingInspectData.first, m_pendingInspectData.second);

    for (Vector<pair<long, String>>::iterator it = m_pendingEvaluateTestCommands.begin(); m_frontendDispatcher && it != m_pendingEvaluateTestCommands.end(); ++it)
        m_frontendDispatcher->evaluateForTestInFrontend(static_cast<int>((*it).first), (*it).second);
    m_pendingEvaluateTestCommands.clear();
}

void InspectorAgent::disable(ErrorString*)
{
    m_enabled = false;
}

void InspectorAgent::evaluateForTestInFrontend(long callId, const String& script)
{
    if (m_enabled && m_frontendDispatcher)
        m_frontendDispatcher->evaluateForTestInFrontend(static_cast<int>(callId), script);
    else
        m_pendingEvaluateTestCommands.append(pair<long, String>(callId, script));
}

void InspectorAgent::inspect(PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> objectToInspect, PassRefPtr<InspectorObject> hints)
{
    if (m_enabled && m_frontendDispatcher) {
        m_frontendDispatcher->inspect(objectToInspect, hints);
        m_pendingInspectData.first = 0;
        m_pendingInspectData.second = 0;
        return;
    }
    m_pendingInspectData.first = objectToInspect;
    m_pendingInspectData.second = hints;
}

bool InspectorAgent::developerExtrasEnabled() const
{
    if (!m_inspectedPage)
        return false;
    return m_inspectedPage->settings().developerExtrasEnabled();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
