/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMWindow.h"

#include "BackForwardController.h"
#include "BarProp.h"
#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContentExtensionActions.h"
#include "ContentExtensionRule.h"
#include "Crypto.h"
#include "CustomElementRegistry.h"
#include "DOMApplicationCache.h"
#include "DOMSelection.h"
#include "DOMStringList.h"
#include "DOMTimer.h"
#include "DOMTokenList.h"
#include "DOMURL.h"
#include "DOMWindowExtension.h"
#include "DOMWindowNotifications.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "History.h"
#include "InspectorInstrumentation.h"
#include "JSMainThreadExecState.h"
#include "Language.h"
#include "Location.h"
#include "MainFrame.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MessageEvent.h"
#include "Navigator.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "Performance.h"
#include "ResourceLoadInfo.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "ScheduledAction.h"
#include "Screen.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StyleMedia.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "SuddenTermination.h"
#include "URL.h"
#include "UserGestureIndicator.h"
#include "WebKitPoint.h"
#include "WindowFeatures.h"
#include "WindowFocusAllowedIndicator.h"
#include <algorithm>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <memory>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "UserContentController.h"
#include "UserMessageHandlerDescriptor.h"
#include "WebKitNamespace.h"
#endif

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "RequestAnimationFrameCallback.h"
#endif

#if ENABLE(GAMEPAD)
#include "GamepadManager.h"
#endif

#if PLATFORM(IOS)
#if ENABLE(GEOLOCATION)
#include "NavigatorGeolocation.h"
#endif
#include "WKContentObservation.h"
#endif

using namespace Inspector;

namespace WebCore {

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow& window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, DOMWindow& source, std::unique_ptr<MessagePortChannelArray> channels, RefPtr<SecurityOrigin>&& targetOrigin, RefPtr<ScriptCallStack>&& stackTrace)
        : m_window(window)
        , m_message(message)
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_channels(WTFMove(channels))
        , m_targetOrigin(WTFMove(targetOrigin))
        , m_stackTrace(stackTrace)
        , m_userGestureToForward(UserGestureIndicator::currentUserGesture())
    {
    }

    Ref<MessageEvent> event(ScriptExecutionContext& context)
    {
        return MessageEvent::create(MessagePort::entanglePorts(context, WTFMove(m_channels)), WTFMove(m_message), m_origin, { }, MessageEventSource(RefPtr<DOMWindow>(WTFMove(m_source))));
    }

    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }

private:
    void fired() override
    {
        // This object gets deleted when std::unique_ptr falls out of scope..
        std::unique_ptr<PostMessageTimer> timer(this);
        
        UserGestureIndicator userGestureIndicator(m_userGestureToForward);
        m_window->postMessageTimerFired(*timer);
    }

    Ref<DOMWindow> m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    Ref<DOMWindow> m_source;
    std::unique_ptr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtr<ScriptCallStack> m_stackTrace;
    RefPtr<UserGestureToken> m_userGestureToForward;
};

typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithUnloadEventListeners;
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithBeforeUnloadEventListeners;
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static void addBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    Frame* frame = window->frame();
    if (!frame)
        return false;
    if (!frame->page())
        return false;
    return frame->isMainFrame();
}

bool DOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto& window : set)
        windows.uncheckedAppend(*window.key);

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        Frame* frame = window->frame();
        if (!frame)
            continue;

        if (!frame->loader().shouldClose())
            return false;

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
    return true;
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<DOMWindow*>(this));
}

void DOMWindow::dispatchAllPendingUnloadEvents()
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto& keyValue : set)
        windows.uncheckedAppend(*keyValue.key);

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        window->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), window->document());
        window->dispatchEvent(Event::create(eventNames().unloadEvent, false, false), window->document());

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect DOMWindow::adjustWindowRect(Page* page, const FloatRect& pendingChanges)
{
    ASSERT(page);

    FloatRect screen = screenAvailableRect(page->mainFrame().view());
    FloatRect window = page->chrome().windowRect();

    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(std::isfinite(screen.x()));
    ASSERT(std::isfinite(screen.y()));
    ASSERT(std::isfinite(screen.width()));
    ASSERT(std::isfinite(screen.height()));
    ASSERT(std::isfinite(window.x()));
    ASSERT(std::isfinite(window.y()));
    ASSERT(std::isfinite(window.width()));
    ASSERT(std::isfinite(window.height()));

    // Update window values if new requested values are not NaN.
    if (!std::isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!std::isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!std::isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!std::isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());

    FloatSize minimumSize = page->chrome().client().minimumWindowSize();
    window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool DOMWindow::allowPopUp(Frame* firstFrame)
{
    ASSERT(firstFrame);
    
    auto& settings = firstFrame->settings();

    if (ScriptController::processingUserGesture() || settings.allowWindowOpenWithoutUserGesture())
        return true;

    return settings.javaScriptCanOpenWindowsAutomatically();
}

bool DOMWindow::allowPopUp()
{
    return m_frame && allowPopUp(m_frame);
}

bool DOMWindow::canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;

    // Override support for layout testing purposes.
    if (auto* document = frame->document()) {
        if (auto* window = document->domWindow()) {
            if (window->m_canShowModalDialogOverride)
                return window->m_canShowModalDialogOverride.value();
        }
    }

    auto* page = frame->page();
    return page ? page->chrome().canRunModal() : false;
}

static void languagesChangedCallback(void* context)
{
    static_cast<DOMWindow*>(context)->languagesChanged();
}

void DOMWindow::setCanShowModalDialogOverride(bool allow)
{
    m_canShowModalDialogOverride = allow;
}

DOMWindow::DOMWindow(Document* document)
    : ContextDestructionObserver(document)
    , FrameDestructionObserver(document->frame())
    , m_weakPtrFactory(this)
{
    ASSERT(frame());
    ASSERT(DOMWindow::document());

    addLanguageChangeObserver(this, &languagesChangedCallback);
}

void DOMWindow::didSecureTransitionTo(Document* document)
{
    observeContext(document);
}

DOMWindow::~DOMWindow()
{
#ifndef NDEBUG
    if (!m_suspendedForDocumentSuspension) {
        ASSERT(!m_screen);
        ASSERT(!m_history);
        ASSERT(!m_crypto);
        ASSERT(!m_locationbar);
        ASSERT(!m_menubar);
        ASSERT(!m_personalbar);
        ASSERT(!m_scrollbars);
        ASSERT(!m_statusbar);
        ASSERT(!m_toolbar);
        ASSERT(!m_navigator);
#if ENABLE(WEB_TIMING)
        ASSERT(!m_performance);
#endif
        ASSERT(!m_location);
        ASSERT(!m_media);
        ASSERT(!m_sessionStorage);
        ASSERT(!m_localStorage);
        ASSERT(!m_applicationCache);
    }
#endif

    if (m_suspendedForDocumentSuspension)
        willDestroyCachedFrame();
    else
        willDestroyDocumentInFrame();

    // As the ASSERTs above indicate, this reset should only be necessary if this DOMWindow is suspended for the page cache.
    // But we don't want to risk any of these objects hanging around after we've been destroyed.
    resetDOMWindowProperties();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);

#if ENABLE(GAMEPAD)
    if (m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
#endif

    removeLanguageChangeObserver(this);
}

DOMWindow* DOMWindow::toDOMWindow()
{
    return this;
}

RefPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* DOMWindow::page()
{
    return frame() ? frame()->page() : nullptr;
}

void DOMWindow::frameDestroyed()
{
    Ref<DOMWindow> protectedThis(*this);

    willDestroyDocumentInFrame();
    FrameDestructionObserver::frameDestroyed();
    resetDOMWindowProperties();
    JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(this);
}

void DOMWindow::willDetachPage()
{
    InspectorInstrumentation::frameWindowDiscarded(m_frame, this);
}

void DOMWindow::willDestroyCachedFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInCachedFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->willDestroyGlobalObjectInCachedFrame();
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->willDestroyGlobalObjectInFrame();
}

void DOMWindow::willDetachDocumentFromFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->willDetachGlobalObjectFromFrame();
}

#if ENABLE(GAMEPAD)

void DOMWindow::incrementGamepadEventListenerCount()
{
    if (++m_gamepadEventListenerCount == 1)
        GamepadManager::singleton().registerDOMWindow(this);
}

void DOMWindow::decrementGamepadEventListenerCount()
{
    ASSERT(m_gamepadEventListenerCount);

    if (!--m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
}

#endif

void DOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void DOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void DOMWindow::resetUnlessSuspendedForDocumentSuspension()
{
    if (m_suspendedForDocumentSuspension)
        return;
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void DOMWindow::suspendForDocumentSuspension()
{
    disconnectDOMWindowProperties();
    m_suspendedForDocumentSuspension = true;
}

void DOMWindow::resumeFromDocumentSuspension()
{
    reconnectDOMWindowProperties();
    m_suspendedForDocumentSuspension = false;
}

void DOMWindow::disconnectDOMWindowProperties()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to disconnectFrameForDocumentSuspension.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->disconnectFrameForDocumentSuspension();
}

void DOMWindow::reconnectDOMWindowProperties()
{
    ASSERT(m_suspendedForDocumentSuspension);
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to reconnectFromPageCache.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->reconnectFrameFromDocumentSuspension(m_frame);
}

void DOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_applicationCache = nullptr;
    m_crypto = nullptr;
    m_history = nullptr;
    m_localStorage = nullptr;
    m_location = nullptr;
    m_locationbar = nullptr;
    m_media = nullptr;
    m_menubar = nullptr;
    m_navigator = nullptr;
    m_personalbar = nullptr;
    m_screen = nullptr;
    m_scrollbars = nullptr;
    m_selection = nullptr;
    m_sessionStorage = nullptr;
    m_statusbar = nullptr;
    m_toolbar = nullptr;

#if ENABLE(WEB_TIMING)
    m_performance = nullptr;
#endif
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    return m_frame && m_frame->document()->domWindow() == this;
}

#if ENABLE(CUSTOM_ELEMENTS)

CustomElementRegistry& DOMWindow::ensureCustomElementRegistry()
{
    if (!m_customElementRegistry)
        m_customElementRegistry = CustomElementRegistry::create(*this);
    return *m_customElementRegistry;
}

#endif

#if ENABLE(ORIENTATION_EVENTS)

int DOMWindow::orientation() const
{
    if (!m_frame)
        return 0;

    return m_frame->orientation();
}

#endif

Screen* DOMWindow::screen() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_screen)
        m_screen = Screen::create(m_frame);
    return m_screen.get();
}

History* DOMWindow::history() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_history)
        m_history = History::create(*m_frame);
    return m_history.get();
}

Crypto* DOMWindow::crypto() const
{
    // FIXME: Why is crypto not available when the window is not currently displayed in a frame?
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_crypto)
        m_crypto = Crypto::create(*document());
    return m_crypto.get();
}

BarProp* DOMWindow::locationbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_locationbar)
        m_locationbar = BarProp::create(m_frame, BarProp::Locationbar);
    return m_locationbar.get();
}

BarProp* DOMWindow::menubar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_menubar)
        m_menubar = BarProp::create(m_frame, BarProp::Menubar);
    return m_menubar.get();
}

BarProp* DOMWindow::personalbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_personalbar)
        m_personalbar = BarProp::create(m_frame, BarProp::Personalbar);
    return m_personalbar.get();
}

BarProp* DOMWindow::scrollbars() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(m_frame, BarProp::Scrollbars);
    return m_scrollbars.get();
}

BarProp* DOMWindow::statusbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_statusbar)
        m_statusbar = BarProp::create(m_frame, BarProp::Statusbar);
    return m_statusbar.get();
}

BarProp* DOMWindow::toolbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_toolbar)
        m_toolbar = BarProp::create(m_frame, BarProp::Toolbar);
    return m_toolbar.get();
}

PageConsoleClient* DOMWindow::console() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    return m_frame->page() ? &m_frame->page()->console() : nullptr;
}

DOMApplicationCache* DOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_applicationCache)
        m_applicationCache = DOMApplicationCache::create(m_frame);
    return m_applicationCache.get();
}

Navigator* DOMWindow::navigator() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_navigator)
        m_navigator = Navigator::create(*m_frame);
    return m_navigator.get();
}

#if ENABLE(WEB_TIMING)

Performance* DOMWindow::performance() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_performance)
        m_performance = Performance::create(*m_frame);
    return m_performance.get();
}

#endif

double DOMWindow::nowTimestamp() const
{
#if ENABLE(WEB_TIMING)
    return performance() ? performance()->now() / 1000 : 0;
#else
    return document() ? document()->monotonicTimestamp() : 0;
#endif
}

Location* DOMWindow::location() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_location)
        m_location = Location::create(m_frame);
    return m_location.get();
}

#if ENABLE(USER_MESSAGE_HANDLERS)

bool DOMWindow::shouldHaveWebKitNamespaceForWorld(DOMWrapperWorld& world)
{
    if (!m_frame)
        return false;

    auto* page = m_frame->page();
    if (!page)
        return false;

    bool hasUserMessageHandler = false;
    page->userContentProvider().forEachUserMessageHandler([&](const UserMessageHandlerDescriptor& descriptor) {
        if (&descriptor.world() == &world) {
            hasUserMessageHandler = true;
            return;
        }
    });

    return hasUserMessageHandler;
}

WebKitNamespace* DOMWindow::webkitNamespace() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    auto* page = m_frame->page();
    if (!page)
        return nullptr;
    if (!m_webkitNamespace)
        m_webkitNamespace = WebKitNamespace::create(*m_frame, page->userContentProvider());
    return m_webkitNamespace.get();
}

#endif

ExceptionOr<Storage*> DOMWindow::sessionStorage() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    auto* document = this->document();
    if (!document)
        return nullptr;

    if (!document->securityOrigin()->canAccessSessionStorage(document->topOrigin()))
        return Exception { SECURITY_ERR };

    if (m_sessionStorage) {
        if (!m_sessionStorage->area().canAccessStorage(m_frame))
        return Exception { SECURITY_ERR };
        return m_sessionStorage.get();
    }

    auto* page = document->page();
    if (!page)
        return nullptr;

    auto storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame))
        return Exception { SECURITY_ERR };

    m_sessionStorage = Storage::create(m_frame, WTFMove(storageArea));
    return m_sessionStorage.get();
}

ExceptionOr<Storage*> DOMWindow::localStorage() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    auto* document = this->document();
    if (!document)
        return nullptr;

    if (!document->securityOrigin()->canAccessLocalStorage(nullptr))
        return Exception { SECURITY_ERR };

    auto* page = document->page();
    // FIXME: We should consider supporting access/modification to local storage
    // after calling window.close(). See <https://bugs.webkit.org/show_bug.cgi?id=135330>.
    if (!page || !page->isClosing()) {
        if (m_localStorage) {
            if (!m_localStorage->area().canAccessStorage(m_frame))
                return Exception { SECURITY_ERR };
            return m_localStorage.get();
        }
    }

    if (!page)
        return nullptr;

    if (page->isClosing())
        return nullptr;

    if (!page->settings().localStorageEnabled())
        return nullptr;

    auto storageArea = page->storageNamespaceProvider().localStorageArea(*document);

    if (!storageArea->canAccessStorage(m_frame))
        return Exception { SECURITY_ERR };

    m_localStorage = Storage::create(m_frame, WTFMove(storageArea));
    return m_localStorage.get();
}

ExceptionOr<void> DOMWindow::postMessage(Ref<SerializedScriptValue>&& message, Vector<RefPtr<MessagePort>>&& ports, const String& targetOrigin, DOMWindow& source)
{
    if (!isCurrentlyDisplayedInFrame())
        return { };

    Document* sourceDocument = source.document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SYNTAX_ERR exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return { };
        target = sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique())
            return Exception { SYNTAX_ERR };
    }

    auto channels = MessagePort::disentanglePorts(WTFMove(ports));
    if (channels.hasException())
        return channels.releaseException();

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return { };
    auto sourceOrigin = sourceDocument->securityOrigin()->toString();

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtr<ScriptCallStack> stackTrace;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(JSMainThreadExecState::currentState(), ScriptCallStack::maxCallStackSizeToCapture);

    // Schedule the message.
    auto* timer = new PostMessageTimer(*this, WTFMove(message), sourceOrigin, source, channels.releaseReturnValue(), WTFMove(target), WTFMove(stackTrace));
    timer->startOneShot(0);

    return { };
}

void DOMWindow::postMessageTimerFired(PostMessageTimer& timer)
{
    if (!document() || !isCurrentlyDisplayedInFrame())
        return;

    dispatchMessageEventWithOriginCheck(timer.targetOrigin(), timer.event(*document()), timer.stackTrace());
}

void DOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, Event& event, PassRefPtr<ScriptCallStack> stackTrace)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            if (PageConsoleClient* pageConsole = console()) {
                String message = makeString("Unable to post message to ", intendedTargetOrigin->toString(), ". Recipient has origin ", document()->securityOrigin()->toString(), ".\n");
                pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message, stackTrace);
            }
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* DOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_selection)
        m_selection = DOMSelection::create(*m_frame);
    return m_selection.get();
}

Element* DOMWindow::frameElement() const
{
    if (!m_frame)
        return nullptr;

    return m_frame->ownerElement();
}

void DOMWindow::focus(DOMWindow& callerWindow)
{
    focus(opener() && opener() != this && &callerWindow == opener());
}

void DOMWindow::focus(bool allowFocus)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    allowFocus = allowFocus || WindowFocusAllowedIndicator::windowFocusAllowed() || !m_frame->settings().windowFocusRestricted();

    // If we're a top level window, bring the window to the front.
    if (m_frame->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!m_frame)
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    Frame* focusedFrame = page->focusController().focusedFrame();
    if (focusedFrame && focusedFrame != m_frame)
        focusedFrame->document()->setFocusedElement(nullptr);

    // setFocusedElement may clear m_frame, so recheck before using it.
    if (m_frame)
        m_frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame->settings().windowFocusRestricted())
        return;

    if (!m_frame->isMainFrame())
        return;

    page->chrome().unfocus();
}

void DOMWindow::close(Document& document)
{
    if (!document.canNavigate(m_frame))
        return;
    close();
}

void DOMWindow::close()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (!m_frame->isMainFrame())
        return;

    bool allowScriptsToCloseWindows = m_frame->settings().allowScriptsToCloseWindows();

    if (!(page->openedByDOM() || page->backForward().count() <= 1 || allowScriptsToCloseWindows)) {
        console()->addMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Can't close the window since it was not opened by JavaScript"));
        return;
    }

    if (!m_frame->loader().shouldClose())
        return;

    page->setIsClosing();
    page->chrome().closeWindowSoon();
}

void DOMWindow::print()
{
    if (!m_frame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.print is not allowed while unloading a page.");
        return;
    }

    if (m_frame->loader().activeDocumentLoader()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(m_frame);
}

void DOMWindow::stop()
{
    if (!m_frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    m_frame->loader().stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.alert is not allowed while unloading a page.");
        return;
    }

    m_frame->document()->updateStyleIfNeeded();

    page->chrome().runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;
    
    auto* page = m_frame->page();
    if (!page)
        return false;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.confirm is not allowed while unloading a page.");
        return false;
    }

    m_frame->document()->updateStyleIfNeeded();

    return page->chrome().runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    auto* page = m_frame->page();
    if (!page)
        return String();

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.prompt is not allowed while unloading a page.");
        return String();
    }

    m_frame->document()->updateStyleIfNeeded();

    String returnValue;
    if (page->chrome().runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog.    
    FindOptions options = (backwards ? Backwards : 0) | (caseSensitive ? 0 : CaseInsensitive) | (wrap ? WrapAround : 0);
    return m_frame->editor().findString(string, options);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
#if PLATFORM(IOS)
    return 0;
#else
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().height());
#endif
}

int DOMWindow::outerWidth() const
{
#if PLATFORM(IOS)
    return 0;
#else
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().width());
#endif
}

int DOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int DOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int DOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollX = view->contentsScrollPosition().x();
    if (!scrollX)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
}

int DOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollY = view->contentsScrollPosition().y();
    if (!scrollY)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
}

bool DOMWindow::closed() const
{
    return !m_frame;
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return m_frame->tree().scopedChildCount();
}

String DOMWindow::name() const
{
    if (!m_frame)
        return String();

    return m_frame->tree().name();
}

void DOMWindow::setName(const String& string)
{
    if (!m_frame)
        return;

    m_frame->tree().setName(string);
}

void DOMWindow::setStatus(const String& string) 
{
    m_status = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_status);
} 
    
void DOMWindow::setDefaultStatus(const String& string) 
{
    m_defaultStatus = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_defaultStatus);
}

DOMWindow* DOMWindow::self() const
{
    if (!m_frame)
        return nullptr;

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    if (!m_frame)
        return nullptr;

    Frame* opener = m_frame->loader().opener();
    if (!opener)
        return nullptr;

    return opener->document()->domWindow();
}

DOMWindow* DOMWindow::parent() const
{
    if (!m_frame)
        return nullptr;

    Frame* parent = m_frame->tree().parent();
    if (parent)
        return parent->document()->domWindow();

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!m_frame)
        return nullptr;

    Page* page = m_frame->page();
    if (!page)
        return nullptr;

    return m_frame->tree().top().document()->domWindow();
}

Document* DOMWindow::document() const
{
    ScriptExecutionContext* context = ContextDestructionObserver::scriptExecutionContext();
    return downcast<Document>(context);
}

RefPtr<StyleMedia> DOMWindow::styleMedia() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return m_media;
}

Ref<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element& element, const String& pseudoElt) const
{
    return CSSComputedStyleDeclaration::create(element, false, pseudoElt);
}

// FIXME: Drop this overload once <rdar://problem/28016778> has been fixed.
ExceptionOr<RefPtr<CSSStyleDeclaration>> DOMWindow::getComputedStyle(Document&, const String&)
{
#if PLATFORM(MAC)
    if (MacApplication::isAppStore()) {
        printErrorMessage(ASCIILiteral("Passing a non-Element as first parameter to window.getComputedStyle() is invalid and always returns null"));
        return nullptr;
    }
#endif
    return Exception { TypeError };
}

RefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement, bool authorOnly) const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoElementType pseudoType = CSSSelector::parsePseudoElementType(pseudoElement.substringSharingImpl(colonStart));
    if (pseudoType == CSSSelector::PseudoElementUnknown && !pseudoElement.isEmpty())
        return nullptr;

    m_frame->document()->styleScope().flushPendingUpdate();

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= StyleResolver::UAAndUserCSSRules;
    if (m_frame->settings().crossOriginCheckInGetMatchedCSSRulesDisabled())
        rulesToInclude |= StyleResolver::CrossOriginCSSRules;

    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);

    auto matchedRules = m_frame->document()->styleScope().resolver().pseudoStyleRulesForElement(element, pseudoId, rulesToInclude);
    if (matchedRules.isEmpty())
        return nullptr;

    RefPtr<StaticCSSRuleList> ruleList = StaticCSSRuleList::create();
    for (auto& rule : matchedRules)
        ruleList->rules().append(rule->createCSSOMWrapper());

    return ruleList;
}

RefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

RefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return WebKitPoint::create(nodePoint.x(), nodePoint.y());
}

double DOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    Page* page = m_frame->page();
    if (!page)
        return 0.0;

    return page->deviceScaleFactor();
}

void DOMWindow::scrollBy(const ScrollToOptions& options) const
{
    return scrollBy(options.left.valueOr(0), options.top.valueOr(0));
}

void DOMWindow::scrollBy(double x, double y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    // Normalize non-finite values (https://drafts.csswg.org/cssom-view/#normalize-non-finite-values).
    x = std::isfinite(x) ? x : 0;
    y = std::isfinite(y) ? y : 0;

    IntSize scaledOffset(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
    view->setContentsScrollPosition(view->contentsScrollPosition() + scaledOffset);
}

void DOMWindow::scrollTo(const ScrollToOptions& options) const
{
    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;

    double x = options.left ? options.left.value() : view->contentsScrollPosition().x();
    double y = options.top ? options.top.value() : view->contentsScrollPosition().y();
    return scrollTo(x, y);
}

void DOMWindow::scrollTo(double x, double y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;

    // Normalize non-finite values (https://drafts.csswg.org/cssom-view/#normalize-non-finite-values).
    x = std::isfinite(x) ? x : 0;
    y = std::isfinite(y) ? y : 0;

    if (!x && !y && view->contentsScrollPosition() == IntPoint(0, 0))
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
    view->setContentsScrollPosition(layoutPos);
}

bool DOMWindow::allowedToChangeWindowGeometry() const
{
    if (!m_frame)
        return false;
    if (!m_frame->page())
        return false;
    if (!m_frame->isMainFrame())
        return false;
    // Prevent web content from tricking the user into initiating a drag.
    if (m_frame->eventHandler().mousePressed())
        return false;
    return true;
}

void DOMWindow::moveBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect sr = screenAvailableRect(page->mainFrame().view());
    fr.setLocation(sr.location());
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

ExceptionOr<int> DOMWindow::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return ExceptionCode { INVALID_ACCESS_ERR };
    return DOMTimer::install(*context, WTFMove(action), std::chrono::milliseconds(timeout), true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
#if PLATFORM(IOS)
    if (m_frame) {
        Document* document = m_frame->document();
        if (timeoutId > 0 && document) {
            DOMTimer* timer = document->findTimeout(timeoutId);
            if (timer && WebThreadContainsObservedContentModifier(timer)) {
                WebThreadRemoveObservedContentModifier(timer);

                if (!WebThreadCountOfObservedContentModifiers()) {
                    if (Page* page = m_frame->page())
                        page->chrome().client().observedContentChange(m_frame);
                }
            }
        }
    }
#endif
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(*context, timeoutId);
}

ExceptionOr<int> DOMWindow::setInterval(std::unique_ptr<ScheduledAction> action, int timeout)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return ExceptionCode { INVALID_ACCESS_ERR };
    return DOMTimer::install(*context, WTFMove(action), std::chrono::milliseconds(timeout), false);
}

void DOMWindow::clearInterval(int timeoutId)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(*context, timeoutId);
}

#if ENABLE(REQUEST_ANIMATION_FRAME)

int DOMWindow::requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int DOMWindow::webkitRequestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void DOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}

#endif

static void didAddStorageEventListener(DOMWindow& window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system. 
    window.localStorage();
    window.sessionStorage();
}

bool DOMWindow::isSameSecurityOriginAsMainFrame() const
{
    if (!m_frame || !m_frame->page() || !document())
        return false;

    if (m_frame->isMainFrame())
        return true;

    Document* mainFrameDocument = m_frame->mainFrame().document();

    if (mainFrameDocument && document()->securityOrigin()->canAccess(mainFrameDocument->securityOrigin()))
        return true;

    return false;
}

bool DOMWindow::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), options))
        return false;

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (eventNames().isWheelEventType(eventType))
            document->didAddWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didAddTouchEventHandler(*document);
        else if (eventType == eventNames().storageEvent)
            didAddStorageEventListener(*this);
    }

    if (eventType == eventNames().unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if ((eventType == eventNames().devicemotionEvent || eventType == eventNames().deviceorientationEvent) && document()) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (eventType == eventNames().deviceorientationEvent)
                document()->deviceOrientationController()->addDeviceEventListener(this);
            else
                document()->deviceMotionController()->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Blocked attempt add device motion or orientation listener from child frame that wasn't the same security origin as the main page."));
    }
#else
    else if (eventType == eventNames().devicemotionEvent && RuntimeEnabledFeatures::sharedFeatures().deviceMotionEnabled()) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (DeviceMotionController* controller = DeviceMotionController::from(page()))
                controller->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Blocked attempt add device motion listener from child frame that wasn't the same security origin as the main page."));
    } else if (eventType == eventNames().deviceorientationEvent && RuntimeEnabledFeatures::sharedFeatures().deviceOrientationEnabled()) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
                controller->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Blocked attempt add device orientation listener from child frame that wasn't the same security origin as the main page."));
    }
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().scrollEvent)
        incrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType))
        ++m_touchEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType))
        ++m_touchEventListenerCount;
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        incrementGamepadEventListenerCount();
#endif
#if ENABLE(PROXIMITY_EVENTS)
    else if (eventType == eventNames().webkitdeviceproximityEvent) {
        if (DeviceProximityController* controller = DeviceProximityController::from(page()))
            controller->addDeviceEventListener(this);
    }
#endif

    return true;
}

#if PLATFORM(IOS)

void DOMWindow::incrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (++m_scrollEventListenerCount == 1 && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page())
            frame->page()->chrome().client().setNeedsScrollNotifications(frame, true);
    }
}

void DOMWindow::decrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (!--m_scrollEventListenerCount && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page() && document->pageCacheState() == Document::NotInPageCache)
            frame->page()->chrome().client().setNeedsScrollNotifications(frame, false);
    }
}

#endif

void DOMWindow::resetAllGeolocationPermission()
{
    // FIXME: Remove PLATFORM(IOS)-guard once we upstream the iOS changes to Geolocation.cpp.
#if ENABLE(GEOLOCATION) && PLATFORM(IOS)
    if (m_navigator)
        NavigatorGeolocation::from(m_navigator.get())->resetAllGeolocationPermission();
#endif
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    if (!EventTarget::removeEventListener(eventType, listener, options.capture))
        return false;

    if (Document* document = this->document()) {
        if (eventNames().isWheelEventType(eventType))
            document->didRemoveWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(*document);
    }

    if (eventType == eventNames().unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().devicemotionEvent && document())
        document()->deviceMotionController()->removeDeviceEventListener(this);
    else if (eventType == eventNames().deviceorientationEvent && document())
        document()->deviceOrientationController()->removeDeviceEventListener(this);
#else
    else if (eventType == eventNames().devicemotionEvent) {
        if (DeviceMotionController* controller = DeviceMotionController::from(page()))
            controller->removeDeviceEventListener(this);
    } else if (eventType == eventNames().deviceorientationEvent) {
        if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
            controller->removeDeviceEventListener(this);
    }
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().scrollEvent)
        decrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        decrementGamepadEventListenerCount();
#endif
#if ENABLE(PROXIMITY_EVENTS)
    else if (eventType == eventNames().webkitdeviceproximityEvent) {
        if (DeviceProximityController* controller = DeviceProximityController::from(page()))
            controller->removeDeviceEventListener(this);
    }
#endif

    return true;
}

void DOMWindow::languagesChanged()
{
    if (auto* document = this->document())
        document->enqueueWindowEvent(Event::create(eventNames().languagechangeEvent, false, false));
}

void DOMWindow::dispatchLoadEvent()
{
    Ref<Event> loadEvent = Event::create(eventNames().loadEvent, false, false);
    if (m_frame && m_frame->loader().documentLoader() && !m_frame->loader().documentLoader()->timing().loadEventStart()) {
        // The DocumentLoader (and thus its LoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = m_frame->loader().documentLoader();
        LoadTiming& timing = documentLoader->timing();
        timing.markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing.markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = m_frame ? m_frame->ownerElement() : nullptr;
    if (ownerElement)
        ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, false, false));

    InspectorInstrumentation::loadEventFired(frame());
}

bool DOMWindow::dispatchEvent(Event& event, EventTarget* target)
{
    Ref<EventTarget> protectedThis(*this);

    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // <http://www.whatwg.org/specs/web-apps/current-work/multipage/history.html#event-pageshow>.
    if (event.eventInterface() == PageTransitionEventInterfaceType) {
        if (event.type() == eventNames().pageshowEvent) {
            if (m_lastPageStatus == PageStatus::Shown)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatus::Shown;
        } else if (event.type() == eventNames().pagehideEvent) {
            if (m_lastPageStatus == PageStatus::Hidden)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatus::Hidden;
        }
    }

    event.setTarget(target ? target : this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEventOnWindow(frame(), event, *this);

    bool result = fireEventListeners(event);

    InspectorInstrumentation::didDispatchEventOnWindow(cookie);

    return result;
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    if (Document* document = this->document()) {
        document->deviceMotionController()->removeAllDeviceEventListeners(this);
        document->deviceOrientationController()->removeAllDeviceEventListeners(this);
    }
#else
    if (DeviceMotionController* controller = DeviceMotionController::from(page()))
        controller->removeAllDeviceEventListeners(this);
    if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
        controller->removeAllDeviceEventListeners(this);
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)

#if PLATFORM(IOS)
    if (m_scrollEventListenerCount) {
        m_scrollEventListenerCount = 1;
        decrementScrollEventListenersCount();
    }
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    m_touchEventListenerCount = 0;
#endif

#if ENABLE(TOUCH_EVENTS)
    if (Document* document = this->document())
        document->didRemoveEventTargetNode(*document);
#endif

#if ENABLE(PROXIMITY_EVENTS)
    if (DeviceProximityController* controller = DeviceProximityController::from(page()))
        controller->removeAllDeviceEventListeners(this);
#endif

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void DOMWindow::captureEvents()
{
    // Not implemented.
}

void DOMWindow::releaseEvents()
{
    // Not implemented.
}

void DOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        if (m_frame->loader().activeDocumentLoader()->mainDocumentError().isNull())
            print();
    }
}

void DOMWindow::setLocation(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& urlString, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    if (!activeDocument->canNavigate(m_frame))
        return;

    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    URL completedURL = firstFrame->document()->completeURL(urlString);
    if (completedURL.isNull())
        return;

    if (isInsecureScriptAccess(activeWindow, completedURL))
        return;

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != LockHistoryBasedOnGestureState || !ScriptController::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    m_frame->navigationScheduler().scheduleLocationChange(activeDocument, activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList);
}

void DOMWindow::printErrorMessage(const String& message)
{
    if (message.isEmpty())
        return;

    if (PageConsoleClient* pageConsole = console())
        pageConsole->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String DOMWindow::crossDomainAccessErrorMessage(const DOMWindow& activeWindow)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    ASSERT(!activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin* activeOrigin = activeWindow.document()->securityOrigin();
    SecurityOrigin* targetOrigin = document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a frame with origin \"" + targetOrigin->toString() + "\". ";

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    URL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || activeWindow.document()->isSandboxed(SandboxOrigin)) {
        message = "Blocked a frame at \"" + SecurityOrigin::create(activeURL).get().toString() + "\" from accessing a frame at \"" + SecurityOrigin::create(targetURL).get().toString() + "\". ";
        if (document()->isSandboxed(SandboxOrigin) && activeWindow.document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " Both frames are sandboxed and lack the \"allow-same-origin\" flag.";
        if (document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.";
        return "Sandbox access violation: " + message + " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.";
    }

    // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
    if (targetOrigin->protocol() != activeOrigin->protocol())
        return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

    // 'document.domain' errors.
    if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", the frame being accessed set it to \"" + targetOrigin->domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
    if (activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
    if (targetOrigin->domainWasSetInDOM())
        return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin->domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";

    // Default.
    return message + "Protocols, domains, and ports must match.";
}

bool DOMWindow::isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString)
{
    if (!protocolIsJavaScript(urlString))
        return false;

    // If this DOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect DOMWindow from
    // Frame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "activeWindow == this" check?
        if (&activeWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow));
    return true;
}

RefPtr<Frame> DOMWindow::createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return nullptr;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return nullptr;

    URL completedURL = urlString.isEmpty() ? URL(ParsedURLString, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        activeWindow.printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return nullptr;
    }

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.loader().outgoingReferrer());

    ResourceRequest request(completedURL, referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, firstFrame.loader().outgoingOrigin());
    FrameLoadRequest frameRequest(activeDocument->securityOrigin(), request, frameName, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ReplaceDocumentIfJavaScriptURL, activeDocument->shouldOpenExternalURLsPolicyToPropagate());

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    RefPtr<Frame> newFrame = WebCore::createWindow(*activeFrame, openerFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return nullptr;

    newFrame->loader().setOpener(&openerFrame);
    newFrame->page()->setOpenedByDOM();

    if (newFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
        return newFrame;

    if (prepareDialogFunction)
        prepareDialogFunction(*newFrame->document()->domWindow());

    if (created) {
        ResourceRequest resourceRequest(completedURL, referrer, UseProtocolCachePolicy);
        FrameLoadRequest frameRequest(activeWindow.document()->securityOrigin(), resourceRequest, "_self", LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, activeDocument->shouldOpenExternalURLsPolicyToPropagate());
        newFrame->loader().changeLocation(frameRequest);
    } else if (!urlString.isEmpty()) {
        LockHistory lockHistory = ScriptController::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->navigationScheduler().scheduleLocationChange(activeWindow.document(), activeWindow.document()->securityOrigin(), completedURL, referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return nullptr;

    return newFrame;
}

RefPtr<DOMWindow> DOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    DOMWindow& activeWindow, DOMWindow& firstWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return nullptr;

    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return nullptr;

#if ENABLE(CONTENT_EXTENSIONS)
    if (firstFrame->document()
        && firstFrame->mainFrame().page()
        && firstFrame->mainFrame().document()
        && firstFrame->mainFrame().document()->loader()) {
        ResourceLoadInfo resourceLoadInfo = { firstFrame->document()->completeURL(urlString), firstFrame->mainFrame().document()->url(), ResourceType::Popup };
        Vector<ContentExtensions::Action> actions = firstFrame->mainFrame().page()->userContentProvider().actionsForResourceLoad(resourceLoadInfo, *firstFrame->mainFrame().document()->loader());
        for (const ContentExtensions::Action& action : actions) {
            if (action.type() == ContentExtensions::ActionType::BlockLoad)
                return nullptr;
        }
    }
#endif

    if (!firstWindow.allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !m_frame->tree().find(frameName))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = nullptr;
    if (frameName == "_top")
        targetFrame = &m_frame->tree().top();
    else if (frameName == "_parent") {
        if (Frame* parent = m_frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = m_frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame))
            return nullptr;

        URL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
            return targetFrame->document()->domWindow();

        if (urlString.isEmpty())
            return targetFrame->document()->domWindow();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        LockHistory lockHistory = ScriptController::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        targetFrame->navigationScheduler().scheduleLocationChange(activeDocument, activeDocument->securityOrigin(), completedURL, firstFrame->loader().outgoingReferrer(),
            lockHistory, LockBackForwardList::No);
        return targetFrame->document()->domWindow();
    }

    RefPtr<Frame> result = createWindow(urlString, frameName, parseWindowFeatures(windowFeaturesString), activeWindow, *firstFrame, *m_frame);
    return result ? result->document()->domWindow() : nullptr;
}

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return;
    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.showModalDialog is not allowed while unloading a page.");
        return;
    }

    if (!canShowModalDialog(m_frame) || !firstWindow.allowPopUp())
        return;

    RefPtr<Frame> dialogFrame = createWindow(urlString, emptyAtom, parseDialogFeatures(dialogFeaturesString, screenAvailableRect(m_frame->view())), activeWindow, *firstFrame, *m_frame, WTFMove(prepareDialogFunction));
    if (!dialogFrame)
        return;
    dialogFrame->page()->chrome().runModal();
}

void DOMWindow::enableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().enableSuddenTermination();
}

void DOMWindow::disableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().disableSuddenTermination();
}

} // namespace WebCore
