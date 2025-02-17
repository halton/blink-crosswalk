/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockWebRTCDTMFSenderHandler_h
#define MockWebRTCDTMFSenderHandler_h

#include "TestCommon.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebNonCopyable.h"
#include "public/platform/WebRTCDTMFSenderHandler.h"
#include "public/platform/WebString.h"
#include "public/testing/WebTask.h"

namespace WebTestRunner {

class WebTestDelegate;

class MockWebRTCDTMFSenderHandler : public WebKit::WebRTCDTMFSenderHandler, public WebKit::WebNonCopyable {
public:
    MockWebRTCDTMFSenderHandler(const WebKit::WebMediaStreamTrack&, WebTestDelegate*);

    virtual void setClient(WebKit::WebRTCDTMFSenderHandlerClient*) OVERRIDE;

    virtual WebKit::WebString currentToneBuffer() OVERRIDE;

    virtual bool canInsertDTMF() OVERRIDE;
    virtual bool insertDTMF(const WebKit::WebString& tones, long duration, long interToneGap) OVERRIDE;

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }
    void clearToneBuffer() { m_toneBuffer.reset(); }

private:
    MockWebRTCDTMFSenderHandler();

    WebKit::WebRTCDTMFSenderHandlerClient* m_client;
    WebKit::WebMediaStreamTrack m_track;
    WebKit::WebString m_toneBuffer;
    WebTaskList m_taskList;
    WebTestDelegate* m_delegate;
};

}

#endif // MockWebRTCDTMFSenderHandler_h
