/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef ApplicationCache_h
#define ApplicationCache_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/EventNames.h"
#include "core/dom/EventTarget.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/DOMWindowProperty.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class Frame;
class KURL;

class ApplicationCache : public ScriptWrappable, public RefCounted<ApplicationCache>, public EventTarget, public DOMWindowProperty {
public:
    static PassRefPtr<ApplicationCache> create(Frame* frame) { return adoptRef(new ApplicationCache(frame)); }
    ~ApplicationCache() { ASSERT(!m_frame); }

    virtual void willDestroyGlobalObjectInFrame() OVERRIDE;

    unsigned short status() const;
    void update(ExceptionState&);
    void swapCache(ExceptionState&);
    void abort();

    // EventTarget impl

    using RefCounted<ApplicationCache>::ref;
    using RefCounted<ApplicationCache>::deref;

    // Explicitly named attribute event listener helpers

    DEFINE_ATTRIBUTE_EVENT_LISTENER(checking);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(noupdate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(downloading);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(updateready);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cached);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(obsolete);

    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    static const AtomicString& toEventType(ApplicationCacheHost::EventID);

private:
    explicit ApplicationCache(Frame*);

    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    ApplicationCacheHost* applicationCacheHost() const;

    EventTargetData m_eventTargetData;
};

} // namespace WebCore

#endif // ApplicationCache_h
