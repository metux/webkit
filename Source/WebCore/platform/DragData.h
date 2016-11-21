/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef DragData_h
#define DragData_h

#include "Color.h"
#include "DragActions.h"
#include "IntPoint.h"

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/text/WTFString.h>

#ifdef __OBJC__ 
#import <Foundation/Foundation.h>
#import <AppKit/NSDragging.h>
typedef id <NSDraggingInfo> DragDataRef;
#else
typedef void* DragDataRef;
#endif

#elif PLATFORM(WIN)
typedef struct IDataObject* DragDataRef;
#include <wtf/text/WTFString.h>
#elif PLATFORM(GTK)
namespace WebCore {
class SelectionData;
}
typedef WebCore::SelectionData* DragDataRef;
#elif PLATFORM(EFL) || PLATFORM(IOS)
typedef void* DragDataRef;
#endif

namespace WebCore {

class URL;

enum DragApplicationFlags {
    DragApplicationNone = 0,
    DragApplicationIsModal = 1,
    DragApplicationIsSource = 2,
    DragApplicationHasAttachedSheet = 4,
    DragApplicationIsCopyKeyDown = 8
};

#if PLATFORM(WIN)
typedef HashMap<unsigned, Vector<String>> DragDataMap;
#endif

class DragData {
public:
    enum FilenameConversionPolicy { DoNotConvertFilenames, ConvertFilenames };

    // clientPosition is taken to be the position of the drag event within the target window, with (0,0) at the top left
    WEBCORE_EXPORT DragData(DragDataRef, const IntPoint& clientPosition, const IntPoint& globalPosition, DragOperation, DragApplicationFlags = DragApplicationNone);
    WEBCORE_EXPORT DragData(const String& dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, DragOperation, DragApplicationFlags = DragApplicationNone);
    // This constructor should used only by WebKit2 IPC because DragData
    // is initialized by the decoder and not in the constructor.
    DragData() { }
#if PLATFORM(WIN)
    WEBCORE_EXPORT DragData(const DragDataMap&, const IntPoint& clientPosition, const IntPoint& globalPosition, DragOperation sourceOperationMask, DragApplicationFlags = DragApplicationNone);
    const DragDataMap& dragDataMap();
    void getDragFileDescriptorData(int& size, String& pathname);
    void getDragFileContentData(int size, void* dataBlob);
#endif
    const IntPoint& clientPosition() const { return m_clientPosition; }
    const IntPoint& globalPosition() const { return m_globalPosition; }
    DragApplicationFlags flags() const { return m_applicationFlags; }
    DragDataRef platformData() const { return m_platformDragData; }
    DragOperation draggingSourceOperationMask() const { return m_draggingSourceOperationMask; }
    bool containsURL(FilenameConversionPolicy = ConvertFilenames) const;
    bool containsPlainText() const;
    bool containsCompatibleContent() const;
    String asURL(FilenameConversionPolicy = ConvertFilenames, String* title = nullptr) const;
    String asPlainText() const;
    void asFilenames(Vector<String>&) const;
    Color asColor() const;
    bool canSmartReplace() const;
    bool containsColor() const;
    bool containsFiles() const;
    unsigned numberOfFiles() const;
#if PLATFORM(MAC)
    const String& pasteboardName() const { return m_pasteboardName; }
#endif

#if PLATFORM(GTK)

    DragData& operator =(const DragData& data)
    {
        m_clientPosition = data.m_clientPosition;
        m_globalPosition = data.m_globalPosition;
        m_platformDragData = data.m_platformDragData;
        m_draggingSourceOperationMask = data.m_draggingSourceOperationMask;
        m_applicationFlags = data.m_applicationFlags;
        return *this;
    }
#endif

private:
    IntPoint m_clientPosition;
    IntPoint m_globalPosition;
    DragDataRef m_platformDragData;
    DragOperation m_draggingSourceOperationMask;
    DragApplicationFlags m_applicationFlags;
#if PLATFORM(MAC)
    String m_pasteboardName;
#endif
#if PLATFORM(WIN)
    DragDataMap m_dragDataMap;
#endif
};
    
}

#endif // !DragData_h
