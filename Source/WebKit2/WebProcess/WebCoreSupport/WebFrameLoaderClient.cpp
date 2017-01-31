/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "AuthenticationManager.h"
#include "DataReference.h"
#include "DrawingArea.h"
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardListItem.h"
#include "InjectedBundleDOMWindowExtension.h"
#include "InjectedBundleNavigationAction.h"
#include "NavigationActionData.h"
#include "PluginView.h"
#include "UserData.h"
#include "WKBundleAPICast.h"
#include "WebAutomationSessionProxy.h"
#include "WebBackForwardListProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebDocumentLoader.h"
#include "WebErrors.h"
#include "WebEvent.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebFullScreenManager.h"
#include "WebIconDatabaseMessages.h"
#include "WebNavigationDataStore.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebsitePolicies.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObject.h>
#include <WebCore/CachedFrame.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLAppletElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MainFrame.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/SubframeLoader.h>
#include <WebCore/UIEventWithKeyState.h>
#include <WebCore/Widget.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

WebFrameLoaderClient::WebFrameLoaderClient()
    : m_frame(0)
    , m_hasSentResponseToPluginView(false)
    , m_didCompletePageTransition(false)
    , m_frameHasCustomContentProvider(false)
    , m_frameCameFromPageCache(false)
{
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}
    
void WebFrameLoaderClient::frameLoaderDestroyed()
{
    m_frame->invalidate();

    // Balances explicit ref() in WebFrame::create().
    m_frame->deref();
}

bool WebFrameLoaderClient::hasHTMLView() const
{
    return !m_frameHasCustomContentProvider;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_frame->page();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent2()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didRemoveFrameFromHierarchy(webPage, m_frame, userData);
}

void WebFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    bool pageIsProvisionallyLoading = false;
    if (FrameLoader* frameLoader = loader->frameLoader())
        pageIsProvisionallyLoading = frameLoader->provisionalDocumentLoader() == loader;

    webPage->injectedBundleResourceLoadClient().didInitiateLoadForResource(webPage, m_frame, identifier, request, pageIsProvisionallyLoading);
    webPage->addResourceRequest(identifier, request);
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().willSendRequestForFrame(webPage, m_frame, identifier, request, redirectResponse);
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return true;

    return webPage->injectedBundleResourceLoadClient().shouldUseCredentialStorage(webPage, m_frame, identifier);
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge& challenge)
{
    // FIXME: Authentication is a per-resource concept, but we don't do per-resource handling in the UIProcess at the API level quite yet.
    // Once we do, we might need to make sure authentication fits with our solution.

    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::singleton().supplement<AuthenticationManager>()->didReceiveAuthenticationChallenge(m_frame, challenge);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long, const ProtectionSpace& protectionSpace)
{
    // The WebKit 2 Networking process asks the UIProcess directly, so the WebContent process should never receive this callback.
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveResponseForResource(webPage, m_frame, identifier, response);
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int dataLength)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveContentLengthForResource(webPage, m_frame, identifier, dataLength);
}

#if ENABLE(DATA_DETECTION)
void WebFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *detectionResults)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->setDataDetectionResults(detectionResults);
}
#endif

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFinishLoadForResource(webPage, m_frame, identifier);
    webPage->removeResourceRequest(identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFailLoadForResource(webPage, m_frame, identifier, error);
    webPage->removeResourceRequest(identifier);
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int /*length*/)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didHandleOnloadEventsForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().provisionalDocumentLoader());
    const String& url = documentLoader.url().string();
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didReceiveServerRedirectForProvisionalLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame(m_frame->frameID(), documentLoader.navigationID(), url, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidChangeProvisionalURL()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().provisionalDocumentLoader());
    webPage->send(Messages::WebPageProxy::DidChangeProvisionalURLForFrame(m_frame->frameID(), documentLoader.navigationID(), documentLoader.url().string()));
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCancelClientRedirectForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double interval, double fireDate)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().willPerformClientRedirectForFrame(webPage, m_frame, url.string(), interval, fireDate);
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(webPage, m_frame, SameDocumentNavigationAnchorNavigation, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(m_frame->frameID(), navigationID, SameDocumentNavigationAnchorNavigation, m_frame->coreFrame()->document()->url().string(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(webPage, m_frame, SameDocumentNavigationSessionStatePush, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(m_frame->frameID(), navigationID, SameDocumentNavigationSessionStatePush, m_frame->coreFrame()->document()->url().string(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(webPage, m_frame, SameDocumentNavigationSessionStateReplace, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(m_frame->frameID(), navigationID, SameDocumentNavigationSessionStateReplace, m_frame->coreFrame()->document()->url().string(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(webPage, m_frame, SameDocumentNavigationSessionStatePop, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(m_frame->frameID(), navigationID, SameDocumentNavigationSessionStatePop, m_frame->coreFrame()->document()->url().string(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebIconDatabase::DidReceiveIconForPageURL(m_frame->url()), 0);
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(FULLSCREEN_API)
    Element* documentElement = m_frame->coreFrame()->document()->documentElement();
    if (documentElement && documentElement->containsFullScreenElement())
        webPage->fullScreenManager()->exitFullScreenForElement(webPage->fullScreenManager()->element());
#endif

    webPage->findController().hideFindUI();
    webPage->sandboxExtensionTracker().didStartProvisionalLoad(m_frame);

    WebDocumentLoader& provisionalLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().provisionalDocumentLoader());
    const String& url = provisionalLoader.url().string();
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didStartProvisionalLoadForFrame(webPage, m_frame, userData);

    String unreachableURL = provisionalLoader.unreachableURL().string();

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidStartProvisionalLoadForFrame(m_frame->frameID(), provisionalLoader.navigationID(), url, unreachableURL, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    // FIXME: Use direction of title.
    webPage->injectedBundleLoaderClient().didReceiveTitleForFrame(webPage, title.string, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidReceiveTitleForFrame(m_frame->frameID(), title.string, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent> hasInsecureContent)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader());
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCommitLoadForFrame(webPage, m_frame, userData);

    webPage->sandboxExtensionTracker().didCommitProvisionalLoad(m_frame);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidCommitLoadForFrame(m_frame->frameID(), documentLoader.navigationID(), documentLoader.response().mimeType(), m_frameHasCustomContentProvider, static_cast<uint32_t>(m_frame->coreFrame()->loader().loadType()), valueOrCompute(documentLoader.response().certificateInfo(), [] { return CertificateInfo(); }), m_frame->coreFrame()->document()->isPluginDocument(), hasInsecureContent, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    webPage->didCommitLoad(m_frame);
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailProvisionalLoadWithErrorForFrame(webPage, m_frame, error, userData);

    webPage->sandboxExtensionTracker().didFailProvisionalLoad(m_frame);

    // FIXME: This is gross. This is necessary because if the client calls WKBundlePageStopLoading() from within the didFailProvisionalLoadWithErrorForFrame
    // injected bundle client call, that will cause the provisional DocumentLoader to be disconnected from the Frame, and didDistroyNavigation message
    // to be sent to the UIProcess (and the destruction of the DocumentLoader). If that happens, and we had captured the navigationID before injected bundle 
    // client call, the DidFailProvisionalLoadForFrame would send a navigationID of a destroyed Navigation, and the UIProcess would not be able to find it
    // in its table.
    //
    // A better solution to this problem would be find a clean way to postpone the disconnection of the DocumentLoader from the Frame until
    // the entire FrameLoaderClient function was complete.
    uint64_t navigationID = 0;
    if (auto documentLoader = m_frame->coreFrame()->loader().provisionalDocumentLoader())
        navigationID = static_cast<WebDocumentLoader*>(documentLoader)->navigationID();

    // Notify the UIProcess.
    WebCore::Frame* coreFrame = m_frame ? m_frame->coreFrame() : nullptr;
    webPage->send(Messages::WebPageProxy::DidFailProvisionalLoadForFrame(m_frame->frameID(), SecurityOriginData::fromFrame(coreFrame), navigationID, m_frame->coreFrame()->loader().provisionalLoadErrorBeingHandledURL(), error, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame, error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailLoadWithErrorForFrame(webPage, m_frame, error, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFailLoadForFrame(m_frame->frameID(), navigationID, error, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame, error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishDocumentLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishDocumentLoadForFrame(m_frame->frameID(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishLoadForFrame(m_frame->frameID(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame);

    webPage->didFinishLoad(m_frame);
}

void WebFrameLoaderClient::forcePageTransitionIfNeeded()
{
    if (m_didCompletePageTransition)
        return;

    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didCompletePageTransition();
    m_didCompletePageTransition = true;
}

void WebFrameLoaderClient::dispatchDidReachLayoutMilestone(LayoutMilestones milestones)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    if (milestones & DidFirstLayout) {
        // FIXME: We should consider removing the old didFirstLayout API since this is doing double duty with the
        // new didLayout API.
        webPage->injectedBundleLoaderClient().didFirstLayoutForFrame(webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

#if PLATFORM(MAC)
        // FIXME: Do this on DidFirstVisuallyNonEmptyLayout when Mac Safari is able to handle it (<rdar://problem/17580021>)
        if (m_frame->isMainFrame() && !m_didCompletePageTransition && !webPage->corePage()->settings().suppressesIncrementalRendering()) {
            webPage->didCompletePageTransition();
            m_didCompletePageTransition = true;
        }
#endif

#if USE(COORDINATED_GRAPHICS)
        // Make sure viewport properties are dispatched on the main frame by the time the first layout happens.
        ASSERT(!webPage->useFixedLayout() || m_frame != m_frame->page()->mainWebFrame() || m_frame->coreFrame()->document()->didDispatchViewportPropertiesChanged());
#endif
    }

    // Send this after DidFirstLayout-specific calls since some clients expect to get those messages first.
    webPage->dispatchDidReachLayoutMilestone(milestones);

    if (milestones & DidFirstVisuallyNonEmptyLayout) {
        if (m_frame->isMainFrame() && !m_didCompletePageTransition && !webPage->corePage()->settings().suppressesIncrementalRendering()) {
            webPage->didCompletePageTransition();
            m_didCompletePageTransition = true;
        }

        // FIXME: We should consider removing the old didFirstVisuallyNonEmptyLayoutForFrame API since this is doing
        // double duty with the new didLayout API.
        webPage->injectedBundleLoaderClient().didFirstVisuallyNonEmptyLayoutForFrame(webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstVisuallyNonEmptyLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    }
}

void WebFrameLoaderClient::dispatchDidLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didLayoutForFrame(webPage, m_frame);

    webPage->recomputeShortCircuitHorizontalWheelEventsState();

#if PLATFORM(IOS)
    webPage->updateSelectionAppearance();
#endif

    // NOTE: Unlike the other layout notifications, this does not notify the
    // the UIProcess for every call.

    if (m_frame == m_frame->page()->mainWebFrame()) {
        // FIXME: Remove at the soonest possible time.
        webPage->send(Messages::WebPageProxy::SetRenderTreeSize(webPage->renderTreeSize()));
        webPage->mainFrameDidLayout();
    }
}

Frame* WebFrameLoaderClient::dispatchCreatePage(const NavigationAction& navigationAction)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return nullptr;

    // Just call through to the chrome client.
    FrameLoadRequest request(m_frame->coreFrame()->document()->securityOrigin(), navigationAction.resourceRequest(), LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, navigationAction.shouldOpenExternalURLsPolicy());
    Page* newPage = webPage->corePage()->chrome().createWindow(*m_frame->coreFrame(), request, WindowFeatures(), navigationAction);
    if (!newPage)
        return nullptr;
    
    return &newPage->mainFrame();
}

void WebFrameLoaderClient::dispatchShow()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->show();
}

void WebFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, FramePolicyFunction function)
{
    WebPage* webPage = m_frame->page();
    if (!webPage) {
        function(PolicyIgnore);
        return;
    }

    if (!request.url().string()) {
        function(PolicyUse);
        return;
    }

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    WKBundlePagePolicyAction policy = webPage->injectedBundlePolicyClient().decidePolicyForResponse(webPage, m_frame, response, request, userData);
    if (policy == WKBundlePagePolicyActionUse) {
        function(PolicyUse);
        return;
    }

    bool canShowMIMEType = webPage->canShowMIMEType(response.mimeType());

    uint64_t listenerID = m_frame->setUpPolicyListener(WTFMove(function));
    bool receivedPolicyAction;
    uint64_t policyAction;
    DownloadID downloadID;

    Ref<WebFrame> protect(*m_frame);
    WebCore::Frame* coreFrame = m_frame->coreFrame();
    if (!webPage->sendSync(Messages::WebPageProxy::DecidePolicyForResponseSync(m_frame->frameID(), SecurityOriginData::fromFrame(coreFrame), response, request, canShowMIMEType, listenerID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())), Messages::WebPageProxy::DecidePolicyForResponseSync::Reply(receivedPolicyAction, policyAction, downloadID), Seconds::infinity(), IPC::SendSyncOption::InformPlatformProcessWillSuspend)) {
        m_frame->didReceivePolicyDecision(listenerID, PolicyIgnore, 0, { });
        return;
    }

    // We call this synchronously because CFNetwork can only convert a loading connection to a download from its didReceiveResponse callback.
    if (receivedPolicyAction)
        m_frame->didReceivePolicyDecision(listenerID, static_cast<PolicyAction>(policyAction), 0, downloadID);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& navigationAction, const ResourceRequest& request, FormState* formState, const String& frameName, FramePolicyFunction function)
{
    WebPage* webPage = m_frame->page();
    if (!webPage) {
        function(PolicyIgnore);
        return;
    }

    RefPtr<API::Object> userData;

    RefPtr<InjectedBundleNavigationAction> action = InjectedBundleNavigationAction::create(m_frame, navigationAction, formState);

    // Notify the bundle client.
    WKBundlePagePolicyAction policy = webPage->injectedBundlePolicyClient().decidePolicyForNewWindowAction(webPage, m_frame, action.get(), request, frameName, userData);
    if (policy == WKBundlePagePolicyActionUse) {
        function(PolicyUse);
        return;
    }


    uint64_t listenerID = m_frame->setUpPolicyListener(WTFMove(function));

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = action->navigationType();
    navigationActionData.modifiers = action->modifiers();
    navigationActionData.mouseButton = action->mouseButton();
    navigationActionData.syntheticClickType = action->syntheticClickType();
    navigationActionData.userGestureTokenIdentifier = WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = webPage->canHandleRequest(request);
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();

    WebCore::Frame* coreFrame = m_frame ? m_frame->coreFrame() : nullptr;
    webPage->send(Messages::WebPageProxy::DecidePolicyForNewWindowAction(m_frame->frameID(), SecurityOriginData::fromFrame(coreFrame), navigationActionData, request, frameName, listenerID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, FormState* formState, FramePolicyFunction function)
{
    WebPage* webPage = m_frame->page();
    if (!webPage) {
        function(PolicyIgnore);
        return;
    }

    // Always ignore requests with empty URLs. 
    if (request.isEmpty()) {
        function(PolicyIgnore);
        return;
    }

    RefPtr<API::Object> userData;

    RefPtr<InjectedBundleNavigationAction> action = InjectedBundleNavigationAction::create(m_frame, navigationAction, formState);

    // Notify the bundle client.
    WKBundlePagePolicyAction policy = webPage->injectedBundlePolicyClient().decidePolicyForNavigationAction(webPage, m_frame, action.get(), request, userData);
    if (policy == WKBundlePagePolicyActionUse) {
        function(PolicyUse);
        return;
    }
    
    uint64_t listenerID = m_frame->setUpPolicyListener(WTFMove(function));
    bool receivedPolicyAction;
    uint64_t newNavigationID;
    uint64_t policyAction;
    DownloadID downloadID;

    RefPtr<WebFrame> originatingFrame;
    switch (action->navigationType()) {
    case NavigationType::LinkClicked:
        if (EventTarget* target = navigationAction.event()->target()) {
            if (Node* node = target->toNode()) {
                if (Frame* frame = node->document().frame())
                    originatingFrame = WebFrame::fromCoreFrame(*frame);
            }
        }
        break;
    case NavigationType::FormSubmitted:
    case NavigationType::FormResubmitted:
        if (formState)
            originatingFrame = WebFrame::fromCoreFrame(*formState->sourceDocument().frame());
        break;
    case NavigationType::BackForward:
    case NavigationType::Reload:
    case NavigationType::Other:
        break;
    }

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = action->navigationType();
    navigationActionData.modifiers = action->modifiers();
    navigationActionData.mouseButton = action->mouseButton();
    navigationActionData.syntheticClickType = action->syntheticClickType();
    navigationActionData.userGestureTokenIdentifier = WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = webPage->canHandleRequest(request);
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();

    WebCore::Frame* coreFrame = m_frame->coreFrame();
    WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().policyDocumentLoader());
    if (!documentLoader)
        documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().documentLoader());

    // Notify the UIProcess.
    Ref<WebFrame> protect(*m_frame);
    WebCore::Frame* originatingCoreFrame = originatingFrame ? originatingFrame->coreFrame() : nullptr;
    WebsitePolicies websitePolicies;
    if (!webPage->sendSync(Messages::WebPageProxy::DecidePolicyForNavigationAction(m_frame->frameID(), SecurityOriginData::fromFrame(coreFrame), documentLoader->navigationID(), navigationActionData, originatingFrame ? originatingFrame->frameID() : 0, SecurityOriginData::fromFrame(originatingCoreFrame), navigationAction.resourceRequest(), request, listenerID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())), Messages::WebPageProxy::DecidePolicyForNavigationAction::Reply(receivedPolicyAction, newNavigationID, policyAction, downloadID, websitePolicies))) {
        m_frame->didReceivePolicyDecision(listenerID, PolicyIgnore, 0, { });
        return;
    }

    // Only setUserContentExtensionsEnabled if it hasn't already been disabled by reloading without content blockers.
    if (documentLoader->userContentExtensionsEnabled())
        documentLoader->setUserContentExtensionsEnabled(websitePolicies.contentBlockersEnabled);

    switch (websitePolicies.autoplayPolicy) {
    case WebsiteAutoplayPolicy::Default:
        documentLoader->setAutoplayPolicy(AutoplayPolicy::Default);
        break;
    case WebsiteAutoplayPolicy::Allow:
        documentLoader->setAutoplayPolicy(AutoplayPolicy::Allow);
        break;
    case WebsiteAutoplayPolicy::AllowWithoutSound:
        documentLoader->setAutoplayPolicy(AutoplayPolicy::AllowWithoutSound);
        break;
    case WebsiteAutoplayPolicy::Deny:
        documentLoader->setAutoplayPolicy(AutoplayPolicy::Deny);
        break;
    }

    // We call this synchronously because WebCore cannot gracefully handle a frame load without a synchronous navigation policy reply.
    if (receivedPolicyAction)
        m_frame->didReceivePolicyDecision(listenerID, static_cast<PolicyAction>(policyAction), newNavigationID, downloadID);
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    m_frame->invalidatePolicyListener();
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundlePolicyClient().unableToImplementPolicy(webPage, m_frame, error, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::UnableToImplementPolicy(m_frame->frameID(), error, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&& formState)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    auto& form = formState->form();

    ASSERT(formState->sourceDocument().frame());
    auto* sourceFrame = WebFrame::fromCoreFrame(*formState->sourceDocument().frame());
    ASSERT(sourceFrame);

    webPage->injectedBundleFormClient().willSendSubmitEvent(webPage, &form, m_frame, sourceFrame, formState->textFieldValues());
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FormState& formState, FramePolicyFunction function)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    auto& form = formState.form();

    auto* sourceFrame = WebFrame::fromCoreFrame(*formState.sourceDocument().frame());
    ASSERT(sourceFrame);

    auto& values = formState.textFieldValues();

    RefPtr<API::Object> userData;
    webPage->injectedBundleFormClient().willSubmitForm(webPage, &form, m_frame, sourceFrame, values, userData);

    uint64_t listenerID = m_frame->setUpPolicyListener(WTFMove(function));

    webPage->send(Messages::WebPageProxy::WillSubmitForm(m_frame->frameID(), sourceFrame->frameID(), values, listenerID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;
    
    m_pluginView->manualLoadDidFail(error);
    m_pluginView = nullptr;
    m_hasSentResponseToPluginView = false;
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::startDownload(const ResourceRequest& request, const String& suggestedName)
{
    m_frame->startDownload(request, suggestedName);
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::willReplaceMultipartContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->willReplaceMultipartContent(*m_frame);
}

void WebFrameLoaderClient::didReplaceMultipartContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->didReplaceMultipartContent(*m_frame);
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView)
        loader->commitData(data, length);

    // If the document is a stand-alone media document, now is the right time to cancel the WebKit load.
    // FIXME: This code should be shared across all ports. <http://webkit.org/b/48762>.
    if (m_frame->coreFrame()->document()->isMediaDocument())
        loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));

    // Calling commitData did not create the plug-in view.
    if (!m_pluginView)
        return;

    if (!m_hasSentResponseToPluginView) {
        m_pluginView->manualLoadDidReceiveResponse(loader->response());
        // manualLoadDidReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
        // to null
        if (!m_pluginView)
            return;
        m_hasSentResponseToPluginView = true;
    }
    m_pluginView->manualLoadDidReceiveData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    if (!m_pluginView) {
        if (m_frameHasCustomContentProvider) {
            WebPage* webPage = m_frame->page();
            if (!webPage)
                return;

            RefPtr<SharedBuffer> mainResourceData = loader->mainResourceData();
            IPC::DataReference dataReference(reinterpret_cast<const uint8_t*>(mainResourceData ? mainResourceData->data() : 0), mainResourceData ? mainResourceData->size() : 0);
            webPage->send(Messages::WebPageProxy::DidFinishLoadingDataForCustomContentProvider(loader->response().suggestedFilename(), dataReference));
        }

        return;
    }

    // If we just received an empty response without any data, we won't have sent a response to the plug-in view.
    // Make sure to do this before calling manualLoadDidFinishLoading.
    if (!m_hasSentResponseToPluginView) {
        m_pluginView->manualLoadDidReceiveResponse(loader->response());

        // Protect against the above call nulling out the plug-in (by trying to cancel the load for example).
        if (!m_pluginView)
            return;
    }

    m_pluginView->manualLoadDidFinishLoading();
    m_pluginView = nullptr;
    m_hasSentResponseToPluginView = false;
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    WebPage* webPage = m_frame->page();
    if (!webPage || !webPage->pageGroup()->isVisibleToHistoryClient())
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader().documentLoader();

    WebNavigationDataStore data;
    data.url = loader->url().string();
    // FIXME: Use direction of title.
    data.title = loader->title().string;
    data.originalRequest = loader->originalRequestCopy();
    data.response = loader->response();

    webPage->send(Messages::WebPageProxy::DidNavigateWithNavigationData(data, m_frame->frameID()));
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebPage* webPage = m_frame->page();
    if (!webPage || !webPage->pageGroup()->isVisibleToHistoryClient())
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader().documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    // Client redirect
    if (!loader->clientRedirectSourceForHistory().isNull()) {
        webPage->send(Messages::WebPageProxy::DidPerformClientRedirect(
            loader->clientRedirectSourceForHistory(), loader->clientRedirectDestinationForHistory(), m_frame->frameID()));
    }

    // Server redirect
    if (!loader->serverRedirectSourceForHistory().isNull()) {
        webPage->send(Messages::WebPageProxy::DidPerformServerRedirect(
            loader->serverRedirectSourceForHistory(), loader->serverRedirectDestinationForHistory(), m_frame->frameID()));
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem* item) const
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return false;
    
    uint64_t itemID = WebBackForwardListProxy::idForItem(item);
    if (!itemID) {
        // We should never be considering navigating to an item that is not actually in the back/forward list.
        ASSERT_NOT_REACHED();
        return false;
    }

    RefPtr<InjectedBundleBackForwardListItem> bundleItem = InjectedBundleBackForwardListItem::create(item);
    RefPtr<API::Object> userData;

    // Ask the bundle client first
    bool shouldGoToBackForwardListItem = webPage->injectedBundleLoaderClient().shouldGoToBackForwardListItem(webPage, bundleItem.get(), userData);
    if (!shouldGoToBackForwardListItem)
        return false;

    webPage->send(Messages::WebPageProxy::WillGoToBackForwardListItem(itemID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    return true;
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didDisplayInsecureContentForFrame(webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidDisplayInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin&, const URL&)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didRunInsecureContentForFrame(webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidRunInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::didDetectXSS(const URL&, bool)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didDetectXSSForFrame(webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidDetectXSSForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return WebKit::cancelledError(request);
}

ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return WebKit::blockedError(request);
}

ResourceError WebFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest& request)
{
    return WebKit::blockedByContentBlockerError(request);
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return WebKit::interruptedForPolicyChangeError(request);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebFrameLoaderClient::blockedByContentFilterError(const ResourceRequest& request)
{
    return WebKit::blockedByContentFilterError(request);
}
#endif

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response)
{
    return WebKit::pluginWillHandleLoadError(response);
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error)
{
    static NeverDestroyed<const ResourceError> cancelledError(this->cancelledError(ResourceRequest()));
    static NeverDestroyed<const ResourceError> pluginWillHandleLoadError(this->pluginWillHandleLoadError(ResourceResponse()));

    if (error.errorCode() == cancelledError.get().errorCode() && error.domain() == cancelledError.get().domain())
        return false;

    if (error.errorCode() == pluginWillHandleLoadError.get().errorCode() && error.domain() == pluginWillHandleLoadError.get().domain())
        return false;

    return true;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMEType(const String& /*MIMEType*/) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& /*MIMEType*/) const
{
    return true;
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    // Note: Can be called multiple times.
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frame->isMainFrame() && !m_didCompletePageTransition) {
        webPage->didCompletePageTransition();
        m_didCompletePageTransition = true;
    }
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem& historyItem)
{
#if PLATFORM(IOS) || PLATFORM(EFL)
    if (m_frame->isMainFrame())
        m_frame->page()->savePageState(historyItem);
#else
    UNUSED_PARAM(historyItem);
#endif
}

void WebFrameLoaderClient::restoreViewState()
{
#if PLATFORM(IOS) || PLATFORM(EFL)
    Frame& frame = *m_frame->coreFrame();
    HistoryItem* currentItem = frame.loader().history().currentItem();
    if (FrameView* view = frame.view()) {
        if (m_frame->isMainFrame())
            m_frame->page()->restorePageState(*currentItem);
        else if (!view->wasScrolledByUser())
            view->setScrollPosition(currentItem->scrollPosition());
    }
#else
    // Inform the UI process of the scale factor.
    double scaleFactor = m_frame->coreFrame()->loader().history().currentItem()->pageScaleFactor();

    // A scale factor of 0 means the history item has the default scale factor, thus we do not need to update it.
    if (scaleFactor)
        m_frame->page()->send(Messages::WebPageProxy::PageScaleFactorDidChange(scaleFactor));

    // FIXME: This should not be necessary. WebCore should be correctly invalidating
    // the view on restores from the back/forward cache.
    if (m_frame->page() && m_frame == m_frame->page()->mainWebFrame())
        m_frame->page()->drawingArea()->setNeedsDisplay();
#endif
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frame->isMainFrame()) {
        webPage->didStartPageTransition();
        m_didCompletePageTransition = false;
    }
}

void WebFrameLoaderClient::didFinishLoad()
{
    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame);
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

Ref<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return m_frame->page()->createDocumentLoader(*m_frame->coreFrame(), request, substituteData);
}

void WebFrameLoaderClient::updateCachedDocumentLoader(WebCore::DocumentLoader& loader)
{
    m_frame->page()->updateCachedDocumentLoader(static_cast<WebDocumentLoader&>(loader), *m_frame->coreFrame());
}

void WebFrameLoaderClient::setTitle(const StringWithDirection& title, const URL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage || !webPage->pageGroup()->isVisibleToHistoryClient())
        return;

    // FIXME: Use direction of title.
    webPage->send(Messages::WebPageProxy::DidUpdateHistoryTitle(title.string, url.string(), m_frame->frameID()));
}

String WebFrameLoaderClient::userAgent(const URL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->userAgent(m_frame, url);
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame* cachedFrame)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    HasInsecureContent hasInsecureContent;
    if (webPage->sendSync(Messages::WebPageProxy::HasInsecureContent(), Messages::WebPageProxy::HasInsecureContent::Reply(hasInsecureContent)))
        cachedFrame->setHasInsecureContent(hasInsecureContent);
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    const ResourceResponse& response = m_frame->coreFrame()->loader().documentLoader()->response();
    m_frameHasCustomContentProvider = m_frame->isMainFrame() && m_frame->page()->shouldUseCustomContentProviderForResponse(response);
    m_frameCameFromPageCache = true;
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    WebPage* webPage = m_frame->page();

    Color backgroundColor = webPage->drawsBackground() ? Color::white : Color::transparent;
    bool isMainFrame = m_frame->isMainFrame();
    bool isTransparent = !webPage->drawsBackground();
    bool shouldUseFixedLayout = isMainFrame && webPage->useFixedLayout();
    bool shouldDisableScrolling = isMainFrame && !webPage->mainFrameIsScrollable();
    bool shouldHideScrollbars = shouldDisableScrolling;
    IntRect fixedVisibleContentRect;

#if USE(COORDINATED_GRAPHICS)
    if (m_frame->coreFrame()->view())
        fixedVisibleContentRect = m_frame->coreFrame()->view()->fixedVisibleContentRect();
    if (shouldUseFixedLayout)
        shouldHideScrollbars = true;
#endif

    const ResourceResponse& response = m_frame->coreFrame()->loader().documentLoader()->response();
    m_frameHasCustomContentProvider = isMainFrame && webPage->shouldUseCustomContentProviderForResponse(response);
    m_frameCameFromPageCache = false;

    ScrollbarMode defaultScrollbarMode = shouldHideScrollbars ? ScrollbarAlwaysOff : ScrollbarAuto;

    m_frame->coreFrame()->createView(webPage->size(), backgroundColor, isTransparent,
        webPage->fixedLayoutSize(), fixedVisibleContentRect, shouldUseFixedLayout,
        defaultScrollbarMode, /* lock */ shouldHideScrollbars, defaultScrollbarMode, /* lock */ shouldHideScrollbars);

    if (int minimumLayoutWidth = webPage->minimumLayoutSize().width()) {
        int minimumLayoutHeight = std::max(webPage->minimumLayoutSize().height(), 1);
        int maximumSize = std::numeric_limits<int>::max();
        m_frame->coreFrame()->view()->enableAutoSizeMode(true, IntSize(minimumLayoutWidth, minimumLayoutHeight), IntSize(maximumSize, maximumSize));

        if (webPage->autoSizingShouldExpandToViewHeight())
            m_frame->coreFrame()->view()->setAutoSizeFixedMinimumHeight(webPage->size().height());
    }

    m_frame->coreFrame()->view()->setProhibitsScrolling(shouldDisableScrolling);
    m_frame->coreFrame()->view()->setVisualUpdatesAllowedByClient(!webPage->shouldExtendIncrementalRenderingSuppression());
#if PLATFORM(COCOA)
    m_frame->coreFrame()->view()->setViewExposedRect(webPage->drawingArea()->viewExposedRect());
#endif
#if PLATFORM(IOS)
    m_frame->coreFrame()->view()->setDelegatesScrolling(true);
#endif

    if (webPage->scrollPinningBehavior() != DoNotPin)
        m_frame->coreFrame()->view()->setScrollPinningBehavior(webPage->scrollPinningBehavior());

#if USE(COORDINATED_GRAPHICS)
    if (shouldUseFixedLayout) {
        m_frame->coreFrame()->view()->setDelegatesScrolling(shouldUseFixedLayout);
        m_frame->coreFrame()->view()->setPaintsEntireContents(shouldUseFixedLayout);
        return;
    }
#endif
}

void WebFrameLoaderClient::didSaveToPageCache()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frame->isMainFrame())
        webPage->send(Messages::WebPageProxy::DidSaveToPageCache());
}

void WebFrameLoaderClient::didRestoreFromPageCache()
{
    m_frameCameFromPageCache = true;
}

void WebFrameLoaderClient::dispatchDidBecomeFrameset(bool value)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->send(Messages::WebPageProxy::FrameDidBecomeFrameSet(m_frame->frameID(), value));
}

bool WebFrameLoaderClient::canCachePage() const
{
    // We cannot cache frames that have custom representations because they are
    // rendered in the UIProcess.
    return !m_frameHasCustomContentProvider;
}

void WebFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader *documentLoader, SessionID sessionID, const ResourceRequest& request, const ResourceResponse& response)
{
    m_frame->convertMainResourceLoadToDownload(documentLoader, sessionID, request, response);
}

RefPtr<Frame> WebFrameLoaderClient::createFrame(const URL& url, const String& name, HTMLFrameOwnerElement& ownerElement,
    const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    auto* webPage = m_frame->page();

    auto subframe = WebFrame::createSubframe(webPage, name, &ownerElement);
    auto* coreSubframe = subframe->coreFrame();
    if (!coreSubframe)
        return nullptr;

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!coreSubframe->page())
        return nullptr;

    m_frame->coreFrame()->loader().loadURLIntoChildFrame(url, referrer, coreSubframe);

    // The frame's onload handler may have removed it from the document.
    if (!subframe->coreFrame())
        return nullptr;
    ASSERT(subframe->coreFrame() == coreSubframe);
    if (!coreSubframe->tree().parent())
        return nullptr;

    return coreSubframe;
}

RefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement& pluginElement, const URL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    ASSERT(paramNames.size() == paramValues.size());
    ASSERT(m_frame->page());

    Plugin::Parameters parameters;
    parameters.url = url;
    parameters.names = paramNames;
    parameters.values = paramValues;
    parameters.mimeType = mimeType;
    parameters.isFullFramePlugin = loadManually;
    parameters.shouldUseManualLoader = parameters.isFullFramePlugin && !m_frameCameFromPageCache;
#if PLATFORM(COCOA)
    parameters.layerHostingMode = m_frame->page()->layerHostingMode();
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    auto plugin = m_frame->page()->createPlugin(m_frame, &pluginElement, parameters, parameters.mimeType);
    if (!plugin)
        return nullptr;

    return PluginView::create(pluginElement, plugin.releaseNonNull(), parameters);
#else
    UNUSED_PARAM(pluginElement);
    return nullptr;
#endif
}

void WebFrameLoaderClient::recreatePlugin(Widget* widget)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    ASSERT(widget);
    ASSERT(widget->isPluginViewBase());
    ASSERT(m_frame->page());

    auto& pluginView = static_cast<PluginView&>(*widget);
    String newMIMEType;
    auto plugin = m_frame->page()->createPlugin(m_frame, pluginView.pluginElement(), pluginView.initialParameters(), newMIMEType);
    pluginView.recreateAndInitialize(plugin.releaseNonNull());
#else
    UNUSED_PARAM(widget);
#endif
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    if (pluginWidget)
        m_pluginView = static_cast<PluginView*>(pluginWidget);
}

#if ENABLE(WEBGL)

WebCore::WebGLLoadPolicy WebFrameLoaderClient::webGLPolicyForURL(const String& url) const
{
    if (auto* webPage = m_frame->page())
        return webPage->webGLPolicyForURL(m_frame, url);

    return WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebFrameLoaderClient::resolveWebGLPolicyForURL(const String& url) const
{
    if (auto* webPage = m_frame->page())
        return webPage->resolveWebGLPolicyForURL(m_frame, url);

    return WebGLAllowCreation;
}

#endif

RefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(const IntSize& pluginSize, HTMLAppletElement& appletElement, const URL&, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    auto plugin = createPlugin(pluginSize, appletElement, URL(), paramNames, paramValues, appletElement.serviceType(), false);
    if (!plugin) {
        if (auto* webPage = m_frame->page()) {
            auto frameURLString = m_frame->coreFrame()->loader().documentLoader()->responseURL().string();
            auto pageURLString = webPage->corePage()->mainFrame().loader().documentLoader()->responseURL().string();
            webPage->send(Messages::WebPageProxy::DidFailToInitializePlugin(appletElement.serviceType(), frameURLString, pageURLString));
        }
    }
    return plugin;
#else
    UNUSED_PARAM(pluginSize);
    UNUSED_PARAM(appletElement);
    UNUSED_PARAM(paramNames);
    UNUSED_PARAM(paramValues);
    return nullptr;
#endif
}

static bool pluginSupportsExtension(const PluginData& pluginData, const String& extension)
{
    ASSERT(extension.convertToASCIILowercase() == extension);
    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    pluginData.getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    for (auto& mimeClassInfo : mimes) {
        if (mimeClassInfo.extensions.contains(extension))
            return true;
    }
    return false;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const URL& url, const String& mimeTypeIn)
{
    // FIXME: This should eventually be merged with WebCore::FrameLoader::defaultObjectContentType.

    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty()) {
        String path = url.path();
        auto dotPosition = path.reverseFind('.');
        if (dotPosition == notFound)
            return ObjectContentType::Frame;
        String extension = path.substring(dotPosition + 1).convertToASCIILowercase();

        // Try to guess the MIME type from the extension.
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(extension);
        if (mimeType.isEmpty()) {
            // Check if there's a plug-in around that can handle the extension.
            if (WebPage* webPage = m_frame->page()) {
                if (pluginSupportsExtension(webPage->corePage()->pluginData(), extension))
                    return ObjectContentType::PlugIn;
            }
            return ObjectContentType::Frame;
        }
    }

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;

    if (WebPage* webPage = m_frame->page()) {
        auto allowedPluginTypes = webFrame()->coreFrame()->loader().subframeLoader().allowPlugins()
            ? PluginData::AllPlugins : PluginData::OnlyApplicationPlugins;
        if (webPage->corePage()->pluginData().supportsMimeType(mimeType, allowedPluginTypes))
            return ObjectContentType::PlugIn;
    }

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentType::Frame;

#if PLATFORM(IOS)
    // iOS can render PDF in <object>/<embed> via PDFDocumentImage.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(mimeType))
        return ObjectContentType::Image;
#endif

    return ObjectContentType::None;
}

String WebFrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().didClearWindowObjectForFrame(webPage, m_frame, world);


    WebAutomationSessionProxy* automationSessionProxy = WebProcess::singleton().automationSessionProxy();
    if (automationSessionProxy && world.isNormal())
        automationSessionProxy->didClearWindowObjectForFrame(*m_frame);

#if HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(EFL))
    // Ensure the accessibility hierarchy is updated.
    webPage->updateAccessibilityTree();
#endif
}


void WebFrameLoaderClient::dispatchGlobalObjectAvailable(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    
    webPage->injectedBundleLoaderClient().globalObjectIsAvailableForFrame(webPage, m_frame, world);
}

void WebFrameLoaderClient::dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDisconnectDOMWindowExtensionFromGlobalObject(webPage, extension);
}

void WebFrameLoaderClient::dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().didReconnectDOMWindowExtensionToGlobalObject(webPage, extension);
}

void WebFrameLoaderClient::dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDestroyGlobalObjectForDOMWindowExtension(webPage, extension);
}

void WebFrameLoaderClient::registerForIconNotification(bool /*listen*/)
{
    notImplemented();
}

#if PLATFORM(COCOA)
    
RemoteAXObjectRef WebFrameLoaderClient::accessibilityRemoteObject() 
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return 0;
    
    return webPage->accessibilityRemoteObject();
}
    
NSCachedURLResponse *WebFrameLoaderClient::willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse* response) const
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return response;

    return webPage->injectedBundleResourceLoadClient().shouldCacheResponse(webPage, m_frame, identifier) ? response : nil;
}

NSDictionary *WebFrameLoaderClient::dataDetectionContext()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return nil;

    return webPage->dataDetectionContext();
}

#endif // PLATFORM(COCOA)

bool WebFrameLoaderClient::shouldAlwaysUsePluginDocument(const String& /*mimeType*/) const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::didChangeScrollOffset()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didChangeScrollOffsetForFrame(m_frame->coreFrame());
}

bool WebFrameLoaderClient::allowScript(bool enabledPerSettings)
{
    if (!enabledPerSettings)
        return false;

    Frame* coreFrame = m_frame->coreFrame();

    if (coreFrame->document()->isPluginDocument()) {
        PluginDocument* pluginDocument = static_cast<PluginDocument*>(coreFrame->document());

        if (pluginDocument->pluginWidget() && pluginDocument->pluginWidget()->isPluginView()) {
            PluginView* pluginView = static_cast<PluginView*>(pluginDocument->pluginWidget());

            if (!pluginView->shouldAllowScripting())
                return false;
        }
    }

    return true;
}

bool WebFrameLoaderClient::shouldForceUniversalAccessFromLocalURL(const WebCore::URL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return false;

    return webPage->injectedBundleLoaderClient().shouldForceUniversalAccessFromLocalURL(webPage, url.string());
}

Ref<FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    return WebFrameNetworkingContext::create(m_frame);
}

#if ENABLE(CONTENT_FILTERING)

void WebFrameLoaderClient::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler unblockHandler)
{
    if (!unblockHandler.needsUIProcess()) {
        m_frame->coreFrame()->loader().policyChecker().setContentFilterUnblockHandler(WTFMove(unblockHandler));
        return;
    }

    if (WebPage* webPage { m_frame->page() })
        webPage->send(Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame(unblockHandler, m_frame->frameID()));
}

#endif

#if ENABLE(REQUEST_AUTOCOMPLETE)

void WebFrameLoaderClient::didRequestAutocomplete(Ref<WebCore::FormState>&&)
{
}

#endif

void WebFrameLoaderClient::prefetchDNS(const String& hostname)
{
    WebProcess::singleton().prefetchDNS(hostname);
}

void WebFrameLoaderClient::didRestoreScrollPosition()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didRestoreScrollPosition();
}

bool WebFrameLoaderClient::useIconLoadingClient()
{
    return m_useIconLoadingClient;
}

void WebFrameLoaderClient::getLoadDecisionForIcon(const LinkIcon& icon, uint64_t callbackID)
{
    if (WebPage* webPage { m_frame->page() })
        webPage->send(Messages::WebPageProxy::GetLoadDecisionForIcon(icon, callbackID));
}

void WebFrameLoaderClient::finishedLoadingIcon(uint64_t loadIdentifier, SharedBuffer* data)
{
    if (WebPage* webPage { m_frame->page() }) {
        if (data)
            webPage->send(Messages::WebPageProxy::FinishedLoadingIcon(loadIdentifier, { reinterpret_cast<const uint8_t*>(data->data()), data->size() }));
        else
            webPage->send(Messages::WebPageProxy::FinishedLoadingIcon(loadIdentifier, { nullptr, 0 }));
    }
}

} // namespace WebKit
