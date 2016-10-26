/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "HTMLCanvasElement.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "ScriptController.h"
#include "Settings.h"
#include <math.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSLock.h>
#include <wtf/RAMSize.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(WEBGL)    
#include "WebGLContextAttributes.h"
#include "WebGLRenderingContextBase.h"
#endif

namespace WebCore {

using namespace HTMLNames;

// These values come from the WhatWG/W3C HTML spec.
const int defaultWidth = 300;
const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size. The maximum canvas size is in device pixels.
#if PLATFORM(IOS)
const unsigned maxCanvasArea = 4096 * 4096;
#elif PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
const unsigned maxCanvasArea = 8192 * 8192;
#else
const unsigned maxCanvasArea = 16384 * 16384;
#endif

#if USE(CG)
// FIXME: It seems strange that the default quality is not the one that is literally named "default".
// Should fix names to make this easier to understand, or write an excellent comment here explaining why not.
const InterpolationQuality defaultInterpolationQuality = InterpolationLow;
#else
const InterpolationQuality defaultInterpolationQuality = InterpolationDefault;
#endif

static size_t activePixelMemory = 0;

HTMLCanvasElement::HTMLCanvasElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_size(defaultWidth, defaultHeight)
{
    ASSERT(hasTagName(canvasTag));
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(Document& document)
{
    return adoptRef(*new HTMLCanvasElement(canvasTag, document));
}

Ref<HTMLCanvasElement> HTMLCanvasElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLCanvasElement(tagName, document));
}

static void removeFromActivePixelMemory(size_t pixelsReleased)
{
    if (!pixelsReleased)
        return;

    if (pixelsReleased < activePixelMemory)
        activePixelMemory -= pixelsReleased;
    else
        activePixelMemory = 0;
}
    
HTMLCanvasElement::~HTMLCanvasElement()
{
    for (auto& observer : m_observers)
        observer->canvasDestroyed(*this);

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.

    releaseImageBufferAndContext();
}

void HTMLCanvasElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == widthAttr || name == heightAttr)
        reset();
    HTMLElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> HTMLCanvasElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    Frame* frame = document().frame();
    if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return createRenderer<RenderHTMLCanvas>(*this, WTFMove(style));
    return HTMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool HTMLCanvasElement::canContainRangeEndPoint() const
{
    return false;
}

bool HTMLCanvasElement::canStartSelection() const
{
    return false;
}

void HTMLCanvasElement::addObserver(CanvasObserver& observer)
{
    m_observers.add(&observer);
}

void HTMLCanvasElement::removeObserver(CanvasObserver& observer)
{
    m_observers.remove(&observer);
}

void HTMLCanvasElement::setHeight(unsigned value)
{
    setAttributeWithoutSynchronization(heightAttr, AtomicString::number(limitToOnlyHTMLNonNegative(value, defaultHeight)));
}

void HTMLCanvasElement::setWidth(unsigned value)
{
    setAttributeWithoutSynchronization(widthAttr, AtomicString::number(limitToOnlyHTMLNonNegative(value, defaultWidth)));
}

#if ENABLE(WEBGL)
static bool requiresAcceleratedCompositingForWebGL()
{
#if PLATFORM(GTK) || PLATFORM(EFL)
    return false;
#else
    return true;
#endif

}
static bool shouldEnableWebGL(Settings* settings)
{
    if (!settings)
        return false;

    if (!settings->webGLEnabled())
        return false;

    if (!requiresAcceleratedCompositingForWebGL())
        return true;

    return settings->acceleratedCompositingEnabled();
}
#endif

static inline size_t maxActivePixelMemory()
{
    static size_t maxPixelMemory;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        maxPixelMemory = std::max(ramSize() / 4, 2151 * MB);
    });
    return maxPixelMemory;
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type, CanvasContextAttributes* attrs)
{
    if (is2dType(type)) {
        if (m_context && !m_context->is2d())
            return nullptr;
        if (!m_context) {
            bool usesDashboardCompatibilityMode = false;
#if ENABLE(DASHBOARD_SUPPORT)
            if (Settings* settings = document().settings())
                usesDashboardCompatibilityMode = settings->usesDashboardBackwardCompatibilityMode();
#endif

            // Make sure we don't use more pixel memory than the system can support.
            size_t requestedPixelMemory = 4 * width() * height();
            if (activePixelMemory + requestedPixelMemory > maxActivePixelMemory()) {
                StringBuilder stringBuilder;
                stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
                stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
                stringBuilder.appendLiteral(" MB).");
                document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
                return nullptr;
            }

            m_context = std::make_unique<CanvasRenderingContext2D>(*this, document().inQuirksMode(), usesDashboardCompatibilityMode);

            downcast<CanvasRenderingContext2D>(*m_context).setUsesDisplayListDrawing(m_usesDisplayListDrawing);
            downcast<CanvasRenderingContext2D>(*m_context).setTracksDisplayListReplay(m_tracksDisplayListReplay);

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
            // Need to make sure a RenderLayer and compositing layer get created for the Canvas
            invalidateStyleAndLayerComposition();
#endif
        }
        return m_context.get();
    }
#if ENABLE(WEBGL)
    if (shouldEnableWebGL(document().settings())) {

        if (is3dType(type)) {
            if (m_context && !m_context->is3d())
                return nullptr;
            if (!m_context) {
                m_context = WebGLRenderingContextBase::create(this, static_cast<WebGLContextAttributes*>(attrs), type);
                if (m_context) {
                    // Need to make sure a RenderLayer and compositing layer get created for the Canvas
                    invalidateStyleAndLayerComposition();
                }
            }
            return m_context.get();
        }
    }
#else
    UNUSED_PARAM(attrs);
#endif
    return nullptr;
}

bool HTMLCanvasElement::is2dType(const String& type)
{
    return type == "2d";
}

#if ENABLE(WEBGL)
bool HTMLCanvasElement::is3dType(const String& type)
{
    // Retain support for the legacy "webkit-3d" name.
    return type == "webgl" || type == "experimental-webgl"
#if ENABLE(WEBGL2)
        || type == "webgl2"
#endif
        || type == "webkit-3d";
}
#endif

void HTMLCanvasElement::didDraw(const FloatRect& rect)
{
    clearCopiedImage();

    FloatRect dirtyRect = rect;
    if (RenderBox* ro = renderBox()) {
        FloatRect destRect = ro->contentBoxRect();
        // Inflate dirty rect to cover antialiasing on image buffers.
        if (drawingContext() && drawingContext()->shouldAntialias())
            dirtyRect.inflate(1);
        FloatRect r = mapRect(dirtyRect, FloatRect(0, 0, size().width(), size().height()), destRect);
        r.intersect(destRect);
        if (r.isEmpty() || m_dirtyRect.contains(r))
            return;

        m_dirtyRect.unite(r);
        ro->repaintRectangle(enclosingIntRect(m_dirtyRect));
    }
    notifyObserversCanvasChanged(dirtyRect);
}

void HTMLCanvasElement::notifyObserversCanvasChanged(const FloatRect& rect)
{
    for (auto& observer : m_observers)
        observer->canvasChanged(*this, rect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset)
        return;

    bool hadImageBuffer = hasCreatedImageBuffer();

    int w = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(widthAttr), defaultWidth);
    int h = limitToOnlyHTMLNonNegative(attributeWithoutSynchronization(heightAttr), defaultHeight);

    if (m_contextStateSaver) {
        // Reset to the initial graphics context state.
        m_contextStateSaver->restore();
        m_contextStateSaver->save();
    }

    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).reset();

    IntSize oldSize = size();
    IntSize newSize(w, h);
    // If the size of an existing buffer matches, we can just clear it instead of reallocating.
    // This optimization is only done for 2D canvases for now.
    if (m_hasCreatedImageBuffer && oldSize == newSize && m_context && m_context->is2d()) {
        if (!m_didClearImageBuffer)
            clearImageBuffer();
        return;
    }

    setSurfaceSize(newSize);

#if ENABLE(WEBGL)
    if (is3D() && oldSize != size())
        static_cast<WebGLRenderingContextBase*>(m_context.get())->reshape(width(), height());
#endif

    auto renderer = this->renderer();
    if (is<RenderHTMLCanvas>(renderer)) {
        auto& canvasRenderer = downcast<RenderHTMLCanvas>(*renderer);
        if (oldSize != size()) {
            canvasRenderer.canvasSizeChanged();
            if (canvasRenderer.hasAcceleratedCompositing())
                canvasRenderer.contentChanged(CanvasChanged);
        }
        if (hadImageBuffer)
            canvasRenderer.repaint();
    }

    for (auto& observer : m_observers)
        observer->canvasResized(*this);
}

bool HTMLCanvasElement::paintsIntoCanvasBuffer() const
{
    ASSERT(m_context);
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (m_context->is2d())
        return true;
#endif

    if (!m_context->isAccelerated())
        return true;

    if (renderBox() && renderBox()->hasAcceleratedCompositing())
        return false;

    return true;
}


void HTMLCanvasElement::paint(GraphicsContext& context, const LayoutRect& r)
{
    // Clear the dirty rect
    m_dirtyRect = FloatRect();

    if (context.paintingDisabled())
        return;
    
    if (m_context) {
        if (!paintsIntoCanvasBuffer() && !document().printing())
            return;

        m_context->paintRenderingResultsToCanvas();
    }

    if (hasCreatedImageBuffer()) {
        ImageBuffer* imageBuffer = buffer();
        if (imageBuffer) {
            if (m_presentedImage) {
                ImageOrientationDescription orientationDescription;
#if ENABLE(CSS_IMAGE_ORIENTATION)
                orientationDescription.setImageOrientationEnum(renderer()->style().imageOrientation());
#endif 
                context.drawImage(*m_presentedImage, snappedIntRect(r), ImagePaintingOptions(orientationDescription));
            } else
                context.drawImageBuffer(*imageBuffer, snappedIntRect(r));
        }
    }

#if ENABLE(WEBGL)    
    if (is3D())
        static_cast<WebGLRenderingContextBase*>(m_context.get())->markLayerComposited();
#endif
}

#if ENABLE(WEBGL)
bool HTMLCanvasElement::is3D() const
{
    return m_context && m_context->is3d();
}
#endif

void HTMLCanvasElement::makeRenderingResultsAvailable()
{
    if (m_context)
        m_context->paintRenderingResultsToCanvas();
}

void HTMLCanvasElement::makePresentationCopy()
{
    if (!m_presentedImage) {
        // The buffer contains the last presented data, so save a copy of it.
        m_presentedImage = buffer()->copyImage(CopyBackingStore, Unscaled);
    }
}

void HTMLCanvasElement::clearPresentationCopy()
{
    m_presentedImage = nullptr;
}

void HTMLCanvasElement::releaseImageBufferAndContext()
{
    m_contextStateSaver = nullptr;
    setImageBuffer(nullptr);
}
    
void HTMLCanvasElement::setSurfaceSize(const IntSize& size)
{
    m_size = size;
    m_hasCreatedImageBuffer = false;
    releaseImageBufferAndContext();
    clearCopiedImage();
}

String HTMLCanvasElement::toEncodingMimeType(const String& mimeType)
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return ASCIILiteral("image/png");
    return mimeType.convertToASCIILowercase();
}

String HTMLCanvasElement::toDataURL(const String& mimeType, const double* quality, ExceptionCode& ec)
{
    if (!m_originClean) {
        ec = SECURITY_ERR;
        return String();
    }

    if (m_size.isEmpty() || !buffer())
        return ASCIILiteral("data:,");

    String encodingMIMEType = toEncodingMimeType(mimeType);

#if USE(CG)
    // Try to get ImageData first, as that may avoid lossy conversions.
    if (auto imageData = getImageData())
        return ImageDataToDataURL(*imageData, encodingMIMEType, quality);
#endif

    makeRenderingResultsAvailable();

    return buffer()->toDataURL(encodingMIMEType, quality);
}

RefPtr<ImageData> HTMLCanvasElement::getImageData()
{
#if ENABLE(WEBGL)
    if (!is3D())
        return nullptr;

    WebGLRenderingContextBase* ctx = static_cast<WebGLRenderingContextBase*>(m_context.get());

    return ctx->paintRenderingResultsToImageData();
#else
    return nullptr;
#endif
}

FloatRect HTMLCanvasElement::convertLogicalToDevice(const FloatRect& logicalRect) const
{
    FloatRect deviceRect(logicalRect);

    float x = floorf(deviceRect.x());
    float y = floorf(deviceRect.y());
    float w = ceilf(deviceRect.maxX() - x);
    float h = ceilf(deviceRect.maxY() - y);
    deviceRect.setX(x);
    deviceRect.setY(y);
    deviceRect.setWidth(w);
    deviceRect.setHeight(h);

    return deviceRect;
}

FloatSize HTMLCanvasElement::convertLogicalToDevice(const FloatSize& logicalSize) const
{
    float width = ceilf(logicalSize.width());
    float height = ceilf(logicalSize.height());
    return FloatSize(width, height);
}

FloatSize HTMLCanvasElement::convertDeviceToLogical(const FloatSize& deviceSize) const
{
    float width = ceilf(deviceSize.width());
    float height = ceilf(deviceSize.height());
    return FloatSize(width, height);
}

SecurityOrigin* HTMLCanvasElement::securityOrigin() const
{
    return document().securityOrigin();
}

bool HTMLCanvasElement::shouldAccelerate(const IntSize& size) const
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    UNUSED_PARAM(size);
    return document().settings() && document().settings()->canvasUsesAcceleratedDrawing();
#elif ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context && !m_context->is2d())
        return false;

    Settings* settings = document().settings();
    if (!settings || !settings->accelerated2dCanvasEnabled())
        return false;

    // Do not use acceleration for small canvas.
    if (size.width() * size.height() < settings->minimumAccelerated2dCanvasSize())
        return false;

    return true;
#else
    UNUSED_PARAM(size);
    return false;
#endif
}

size_t HTMLCanvasElement::memoryCost() const
{
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->memoryCost();
}

size_t HTMLCanvasElement::externalMemoryCost() const
{
    if (!m_imageBuffer)
        return 0;
    return m_imageBuffer->externalMemoryCost();
}

void HTMLCanvasElement::setUsesDisplayListDrawing(bool usesDisplayListDrawing)
{
    if (usesDisplayListDrawing == m_usesDisplayListDrawing)
        return;
    
    m_usesDisplayListDrawing = usesDisplayListDrawing;

    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).setUsesDisplayListDrawing(m_usesDisplayListDrawing);
}

void HTMLCanvasElement::setTracksDisplayListReplay(bool tracksDisplayListReplay)
{
    if (tracksDisplayListReplay == m_tracksDisplayListReplay)
        return;

    m_tracksDisplayListReplay = tracksDisplayListReplay;

    if (is<CanvasRenderingContext2D>(m_context.get()))
        downcast<CanvasRenderingContext2D>(*m_context).setTracksDisplayListReplay(m_tracksDisplayListReplay);
}

String HTMLCanvasElement::displayListAsText(DisplayList::AsTextFlags flags) const
{
    if (is<CanvasRenderingContext2D>(m_context.get()))
        return downcast<CanvasRenderingContext2D>(*m_context).displayListAsText(flags);

    return String();
}

String HTMLCanvasElement::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
{
    if (is<CanvasRenderingContext2D>(m_context.get()))
        return downcast<CanvasRenderingContext2D>(*m_context).replayDisplayListAsText(flags);

    return String();
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!m_imageBuffer);

    m_hasCreatedImageBuffer = true;
    m_didClearImageBuffer = true;

    FloatSize logicalSize = size();
    FloatSize deviceSize = convertLogicalToDevice(logicalSize);
    if (!deviceSize.isExpressibleAsIntSize())
        return;

    if (deviceSize.width() * deviceSize.height() > maxCanvasArea) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Canvas area exceeds the maximum limit (width * height > ");
        stringBuilder.appendNumber(maxCanvasArea);
        stringBuilder.appendLiteral(").");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return;
    }
    
    // Make sure we don't use more pixel memory than the system can support.
    size_t requestedPixelMemory = 4 * width() * height();
    if (activePixelMemory + requestedPixelMemory > maxActivePixelMemory()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("Total canvas memory use exceeds the maximum limit (");
        stringBuilder.appendNumber(maxActivePixelMemory() / 1024 / 1024);
        stringBuilder.appendLiteral(" MB).");
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, stringBuilder.toString());
        return;
    }

    IntSize bufferSize(deviceSize.width(), deviceSize.height());
    if (!bufferSize.width() || !bufferSize.height())
        return;

    RenderingMode renderingMode = shouldAccelerate(bufferSize) ? Accelerated : Unaccelerated;

    setImageBuffer(ImageBuffer::create(size(), renderingMode));
    if (!m_imageBuffer)
        return;
    m_imageBuffer->context().setShadowsIgnoreTransforms(true);
    m_imageBuffer->context().setImageInterpolationQuality(defaultInterpolationQuality);
    if (document().settings() && !document().settings()->antialiased2dCanvasEnabled())
        m_imageBuffer->context().setShouldAntialias(false);
    m_imageBuffer->context().setStrokeThickness(1);
    m_contextStateSaver = std::make_unique<GraphicsContextStateSaver>(m_imageBuffer->context());

    JSC::JSLockHolder lock(scriptExecutionContext()->vm());
    scriptExecutionContext()->vm().heap.reportExtraMemoryAllocated(memoryCost());

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context && m_context->is2d())
        // Recalculate compositing requirements if acceleration state changed.
        const_cast<HTMLCanvasElement*>(this)->invalidateStyleAndLayerComposition();
#endif
}

void HTMLCanvasElement::setImageBuffer(std::unique_ptr<ImageBuffer> buffer) const
{
    removeFromActivePixelMemory(memoryCost());

    m_imageBuffer = WTFMove(buffer);

    activePixelMemory += memoryCost();
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    return buffer() ? &m_imageBuffer->context() : nullptr;
}

GraphicsContext* HTMLCanvasElement::existingDrawingContext() const
{
    if (!m_hasCreatedImageBuffer)
        return nullptr;

    return drawingContext();
}

ImageBuffer* HTMLCanvasElement::buffer() const
{
    if (!m_hasCreatedImageBuffer)
        createImageBuffer();
    return m_imageBuffer.get();
}

Image* HTMLCanvasElement::copiedImage() const
{
    if (!m_copiedImage && buffer()) {
        if (m_context)
            m_context->paintRenderingResultsToCanvas();
        m_copiedImage = buffer()->copyImage(CopyBackingStore, Unscaled);
    }
    return m_copiedImage.get();
}

void HTMLCanvasElement::clearImageBuffer() const
{
    ASSERT(m_hasCreatedImageBuffer);
    ASSERT(!m_didClearImageBuffer);
    ASSERT(m_context);

    m_didClearImageBuffer = true;

    if (is<CanvasRenderingContext2D>(*m_context)) {
        // No need to undo transforms/clip/etc. because we are called right after the context is reset.
        downcast<CanvasRenderingContext2D>(*m_context).clearRect(0, 0, width(), height());
    }
}

void HTMLCanvasElement::clearCopiedImage()
{
    m_copiedImage = nullptr;
    m_didClearImageBuffer = false;
}

AffineTransform HTMLCanvasElement::baseTransform() const
{
    ASSERT(m_hasCreatedImageBuffer);
    FloatSize unscaledSize = size();
    FloatSize deviceSize = convertLogicalToDevice(unscaledSize);
    IntSize size(deviceSize.width(), deviceSize.height());
    AffineTransform transform;
    if (size.width() && size.height())
        transform.scaleNonUniform(size.width() / unscaledSize.width(), size.height() / unscaledSize.height());
    return m_imageBuffer->baseTransform() * transform;
}

}
