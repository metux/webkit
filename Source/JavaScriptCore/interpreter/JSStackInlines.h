/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef JSStackInlines_h
#define JSStackInlines_h

#include "CallFrame.h"
#include "CodeBlock.h"
#include "JSStack.h"
#include "VM.h"

namespace JSC {

inline Register* JSStack::getTopOfFrame(CallFrame* frame)
{
    if (UNLIKELY(!frame))
        return getBaseOfStack();
    return frame->frameExtent();
}

inline Register* JSStack::getTopOfStack()
{
    return getTopOfFrame(m_topCallFrame);
}

inline Register* JSStack::getStartOfFrame(CallFrame* frame)
{
    CallFrame* callerFrame = frame->callerFrameSkippingVMEntrySentinel();
    return getTopOfFrame(callerFrame);
}

inline bool JSStack::entryCheck(class CodeBlock* codeBlock, int argsCount)
{
    Register* oldEnd = getTopOfStack();

    // Ensure that we have enough space for the parameters:
    size_t paddedArgsCount = argsCount;
    if (codeBlock) {
        size_t numParameters = codeBlock->numParameters();
        if (paddedArgsCount < numParameters)
            paddedArgsCount = numParameters;
    }

    Register* newCallFrameSlot = oldEnd - paddedArgsCount - (2 * JSStack::CallFrameHeaderSize) + 1;

#if ENABLE(DEBUG_JSSTACK)
    newCallFrameSlot -= JSStack::FenceSize;
#endif

    Register* newEnd = newCallFrameSlot;
    if (!!codeBlock)
        newEnd += virtualRegisterForLocal(codeBlock->frameRegisterCount()).offset();

    // Ensure that we have the needed stack capacity to push the new frame:
    if (!grow(newEnd))
        return false;

    return true;
}

inline CallFrame* JSStack::pushFrame(class CodeBlock* codeBlock, JSScope* scope, int argsCount, JSObject* callee)
{
    ASSERT(!!scope);
    Register* oldEnd = getTopOfStack();

    // Ensure that we have enough space for the parameters:
    size_t paddedArgsCount = argsCount;
    if (codeBlock) {
        size_t numParameters = codeBlock->numParameters();
        if (paddedArgsCount < numParameters)
            paddedArgsCount = numParameters;
    }

    Register* newCallFrameSlot = oldEnd - paddedArgsCount - (2 * JSStack::CallFrameHeaderSize) + 1;

#if ENABLE(DEBUG_JSSTACK)
    newCallFrameSlot -= JSStack::FenceSize;
#endif

    Register* newEnd = newCallFrameSlot;
    if (!!codeBlock)
        newEnd += virtualRegisterForLocal(codeBlock->frameRegisterCount()).offset();

    // Ensure that we have the needed stack capacity to push the new frame:
    if (!grow(newEnd))
        return 0;

    // Compute the address of the new VM sentinel frame for this invocation:
    CallFrame* newVMEntrySentinelFrame = CallFrame::create(newCallFrameSlot + paddedArgsCount + JSStack::CallFrameHeaderSize);
    ASSERT(!!newVMEntrySentinelFrame);

    // Compute the address of the new frame for this invocation:
    CallFrame* newCallFrame = CallFrame::create(newCallFrameSlot);
    ASSERT(!!newCallFrame);

    // The caller frame should always be the real previous frame on the stack,
    // and not a potential GlobalExec that was passed in. Point callerFrame to
    // the top frame on the stack.
    CallFrame* callerFrame = m_topCallFrame;

    // Initialize the VM sentinel frame header:
    newVMEntrySentinelFrame->initializeVMEntrySentinelFrame(callerFrame);

    // Initialize the callee frame header:
    newCallFrame->init(codeBlock, 0, scope, newVMEntrySentinelFrame, argsCount, callee);

    ASSERT(!!newCallFrame->scope());

    // Pad additional args if needed:
    // Note: we need to subtract 1 from argsCount and paddedArgsCount to
    // exclude the this pointer.
    for (size_t i = argsCount-1; i < paddedArgsCount-1; ++i)
        newCallFrame->setArgument(i, jsUndefined());

    installFence(newCallFrame, __FUNCTION__, __LINE__);
    validateFence(newCallFrame, __FUNCTION__, __LINE__);
    installTrapsAfterFrame(newCallFrame);

    // Push the new frame:
    m_topCallFrame = newCallFrame;

    return newCallFrame;
}

inline void JSStack::popFrame(CallFrame* frame)
{
    validateFence(frame, __FUNCTION__, __LINE__);

    // Pop off the callee frame and the sentinel frame.
    CallFrame* callerFrame = frame->callerFrame()->vmEntrySentinelCallerFrame();

    // Pop to the caller:
    m_topCallFrame = callerFrame;

    // If we are popping the very first frame from the stack i.e. no more
    // frames before this, then we can now safely shrink the stack. In
    // this case, we're shrinking all the way to the beginning since there
    // are no more frames on the stack.
    if (!callerFrame)
        shrink(getBaseOfStack());

    installTrapsAfterFrame(callerFrame);
}

inline void JSStack::shrink(Register* newEnd)
{
    if (newEnd >= m_end)
        return;
    updateStackLimit(newEnd);
    if (m_end == getBaseOfStack() && (m_commitEnd - getBaseOfStack()) >= maxExcessCapacity)
        releaseExcessCapacity();
}

inline bool JSStack::grow(Register* newEnd)
{
    if (newEnd >= m_end)
        return true;
    return growSlowCase(newEnd);
}

inline void JSStack::updateStackLimit(Register* newEnd)
{
    m_end = newEnd;
#if USE(SEPARATE_C_AND_JS_STACK)
    m_vm.setJSStackLimit(newEnd);
#endif
}

#if ENABLE(DEBUG_JSSTACK)
inline JSValue JSStack::generateFenceValue(size_t argIndex)
{
    unsigned fenceBits = 0xfacebad0 | ((argIndex+1) & 0xf);
    JSValue fenceValue = JSValue(fenceBits);
    return fenceValue;
}

// The JSStack fences mechanism works as follows:
// 1. A fence is a number (JSStack::FenceSize) of JSValues that are initialized
//    with values generated by JSStack::generateFenceValue().
// 2. When pushFrame() is called, the fence is installed after the max extent
//    of the previous topCallFrame and the last arg of the new frame:
//
//                     | ...                                  |
//                     |--------------------------------------|
//                     | Frame Header of previous frame       |
//                     |--------------------------------------|
//    topCallFrame --> |                                      |
//                     | Locals of previous frame             |
//                     |--------------------------------------|
//                     | *** the Fence ***                    |
//                     |--------------------------------------|
//                     | VM entry sentinel frame header       |
//                     |--------------------------------------|
//                     | Args of new frame                    |
//                     |--------------------------------------|
//                     | Frame Header of new frame            |
//                     |--------------------------------------|
//           frame --> | Locals of new frame                  |
//                     |                                      |
//
// 3. In popFrame() and elsewhere, we can call JSStack::validateFence() to
//    assert that the fence contains the values we expect.

inline void JSStack::installFence(CallFrame* frame, const char *function, int lineNo)
{
    UNUSED_PARAM(function);
    UNUSED_PARAM(lineNo);
    Register* startOfFrame = getStartOfFrame(frame);

    // The last argIndex is at:
    size_t maxIndex = frame->argIndexForRegister(startOfFrame) + 1;
    size_t startIndex = maxIndex - FenceSize;
    for (size_t i = startIndex; i < maxIndex; ++i) {
        JSValue fenceValue = generateFenceValue(i);
        frame->setArgument(i, fenceValue);
    }
}

inline void JSStack::validateFence(CallFrame* frame, const char *function, int lineNo)
{
    UNUSED_PARAM(function);
    UNUSED_PARAM(lineNo);
    ASSERT(!!frame->scope());
    Register* startOfFrame = getStartOfFrame(frame);
    size_t maxIndex = frame->argIndexForRegister(startOfFrame) + 1;
    size_t startIndex = maxIndex - FenceSize;
    for (size_t i = startIndex; i < maxIndex; ++i) {
        JSValue fenceValue = generateFenceValue(i);
        JSValue actualValue = frame->getArgumentUnsafe(i);
        ASSERT(fenceValue == actualValue);
    }
}

// When debugging the JSStack, we install bad values after the extent of the
// topCallFrame at the end of pushFrame() and popFrame(). The intention is
// to trigger crashes in the event that memory in this supposedly unused
// region is read and consumed without proper initialization. After the trap
// words are installed, the stack looks like this:
//
//                     | ...                         |
//                     |-----------------------------|
//                     | Frame Header of frame       |
//                     |-----------------------------|
//    topCallFrame --> |                             |
//                     | Locals of frame             |
//                     |-----------------------------|
//                     | *** Trap words ***          |
//                     |-----------------------------|
//                     | Unused space ...            |
//                     | ...                         |

inline void JSStack::installTrapsAfterFrame(CallFrame* frame)
{
    Register* topOfFrame = getTopOfFrame(frame);
    const int sizeOfTrap = 64;
    int32_t* startOfTrap = reinterpret_cast<int32_t*>(topOfFrame);
    int32_t* endOfTrap = startOfTrap - sizeOfTrap;
    int32_t* endOfCommitedMemory = reinterpret_cast<int32_t*>(m_commitEnd);

    // Make sure we're not exceeding the amount of available memory to write to:
    if (endOfTrap < endOfCommitedMemory)
        endOfTrap = endOfCommitedMemory;

    // Lay the traps:
    int32_t* p = startOfTrap;
    while (p > endOfTrap)
        *p-- = 0xabadcafe; // A bad word to trigger a crash if deref'ed.
}
#endif // ENABLE(DEBUG_JSSTACK)

} // namespace JSC

#endif // JSStackInlines_h
