/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef DOMFileSystem_h
#define DOMFileSystem_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ScriptExecutionContext.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/EntriesCallback.h"

namespace WebCore {

class DirectoryEntry;
class File;
class FileCallback;
class FileEntry;
class FileWriterCallback;

class DOMFileSystem : public DOMFileSystemBase, public ScriptWrappable, public ActiveDOMObject {
public:
    static PassRefPtr<DOMFileSystem> create(ScriptExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    // Creates a new isolated file system for the given filesystemId.
    static PassRefPtr<DOMFileSystem> createIsolatedFileSystem(ScriptExecutionContext*, const String& filesystemId);

    PassRefPtr<DirectoryEntry> root();

    // DOMFileSystemBase overrides.
    virtual void addPendingCallbacks() OVERRIDE;
    virtual void removePendingCallbacks() OVERRIDE;

    void createWriter(const FileEntry*, PassRefPtr<FileWriterCallback>, PassRefPtr<ErrorCallback>);
    void createFile(const FileEntry*, PassRefPtr<FileCallback>, PassRefPtr<ErrorCallback>);

    // Schedule a callback. This should not cross threads (should be called on the same context thread).
    // FIXME: move this to a more generic place.
    template <typename CB, typename CBArg>
    static void scheduleCallback(ScriptExecutionContext*, PassRefPtr<CB>, PassRefPtr<CBArg>);

    template <typename CB, typename CBArg>
    static void scheduleCallback(ScriptExecutionContext*, PassRefPtr<CB>, const CBArg&);

    template <typename CB, typename CBArg>
    void scheduleCallback(PassRefPtr<CB> callback, PassRefPtr<CBArg> callbackArg)
    {
        scheduleCallback(scriptExecutionContext(), callback, callbackArg);
    }

    template <typename CB, typename CBArg>
    void scheduleCallback(PassRefPtr<CB> callback,  const CBArg& callbackArg)
    {
        scheduleCallback(scriptExecutionContext(), callback, callbackArg);
    }

private:
    DOMFileSystem(ScriptExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    // A helper template to schedule a callback task.
    template <typename CB, typename CBArg>
    class DispatchCallbacRefPtrArgTask : public ScriptExecutionContext::Task {
    public:
        DispatchCallbacRefPtrArgTask(PassRefPtr<CB> callback, PassRefPtr<CBArg> arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ScriptExecutionContext*)
        {
            m_callback->handleEvent(m_callbackArg.get());
        }

    private:
        RefPtr<CB> m_callback;
        RefPtr<CBArg> m_callbackArg;
    };

    template <typename CB, typename CBArg>
    class DispatchCallbackNonPtrArgTask : public ScriptExecutionContext::Task {
    public:
        DispatchCallbackNonPtrArgTask(PassRefPtr<CB> callback, const CBArg& arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ScriptExecutionContext*)
        {
            m_callback->handleEvent(m_callbackArg);
        }

    private:
        RefPtr<CB> m_callback;
        CBArg m_callbackArg;
    };
};

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<CB> callback, PassRefPtr<CBArg> arg)
{
    ASSERT(scriptExecutionContext->isContextThread());
    if (callback)
        scriptExecutionContext->postTask(adoptPtr(new DispatchCallbacRefPtrArgTask<CB, CBArg>(callback, arg)));
}

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<CB> callback, const CBArg& arg)
{
    ASSERT(scriptExecutionContext->isContextThread());
    if (callback)
        scriptExecutionContext->postTask(adoptPtr(new DispatchCallbackNonPtrArgTask<CB, CBArg>(callback, arg)));
}

} // namespace

#endif // DOMFileSystem_h
