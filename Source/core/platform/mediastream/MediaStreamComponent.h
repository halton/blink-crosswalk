/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamComponent_h
#define MediaStreamComponent_h

#include "core/platform/audio/AudioSourceProvider.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/WTFString.h"

namespace WebKit {
class WebAudioSourceProvider;
}

namespace WebCore {

class MediaStreamDescriptor;
class MediaStreamSource;

class MediaStreamComponent : public RefCounted<MediaStreamComponent> {
public:
    static PassRefPtr<MediaStreamComponent> create(PassRefPtr<MediaStreamSource>);
    static PassRefPtr<MediaStreamComponent> create(const String& id, PassRefPtr<MediaStreamSource>);
    static PassRefPtr<MediaStreamComponent> create(MediaStreamDescriptor*, PassRefPtr<MediaStreamSource>);

    MediaStreamDescriptor* stream() const { return m_stream; }
    void setStream(MediaStreamDescriptor* stream) { ASSERT(!m_stream && stream); m_stream = stream; }

    MediaStreamSource* source() const { return m_source.get(); }

    String id() const { return m_id; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() { return &m_sourceProvider; }
    void setSourceProvider(WebKit::WebAudioSourceProvider* provider) { m_sourceProvider.wrap(provider); }
#endif // ENABLE(WEB_AUDIO)

private:
    MediaStreamComponent(const String& id, MediaStreamDescriptor*, PassRefPtr<MediaStreamSource>);

#if ENABLE(WEB_AUDIO)
    // AudioSourceProviderImpl wraps a WebAudioSourceProvider::provideInput()
    // calls into chromium to get a rendered audio stream.

    class AudioSourceProviderImpl : public AudioSourceProvider {
    public:
        AudioSourceProviderImpl()
            : m_webAudioSourceProvider(0)
        {
        }

        virtual ~AudioSourceProviderImpl() { }

        // Wraps the given WebKit::WebAudioSourceProvider to WebCore::AudioSourceProvider.
        void wrap(WebKit::WebAudioSourceProvider*);

        // WebCore::AudioSourceProvider
        virtual void provideInput(WebCore::AudioBus*, size_t framesToProcess);

    private:
        WebKit::WebAudioSourceProvider* m_webAudioSourceProvider;
        Mutex m_provideInputLock;
    };

    AudioSourceProviderImpl m_sourceProvider;
#endif // ENABLE(WEB_AUDIO)

    MediaStreamDescriptor* m_stream;
    RefPtr<MediaStreamSource> m_source;
    String m_id;
    bool m_enabled;
};

typedef Vector<RefPtr<MediaStreamComponent> > MediaStreamComponentVector;

} // namespace WebCore

#endif // MediaStreamComponent_h
