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

#ifndef FileSystemCallbacks_h
#define FileSystemCallbacks_h

#include "core/platform/AsyncFileSystemCallbacks.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/FileSystemType.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMFileSystemBase;
class DirectoryReaderBase;
class EntriesCallback;
class EntryCallback;
class ErrorCallback;
struct FileMetadata;
class FileSystemCallback;
class FileWriterBase;
class FileWriterBaseCallback;
class MetadataCallback;
class ScriptExecutionContext;
class VoidCallback;

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
public:
    virtual ~FileSystemCallbacksBase();

    // For ErrorCallback.
    virtual void didFail(int code) OVERRIDE;

    // Other callback methods are implemented by each subclass.

protected:
    FileSystemCallbacksBase(PassRefPtr<ErrorCallback>, DOMFileSystemBase*);
    RefPtr<ErrorCallback> m_errorCallback;
    DOMFileSystemBase* m_fileSystem;
};

// Subclasses ----------------------------------------------------------------

class EntryCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, PassRefPtr<DOMFileSystemBase>, const String& expectedPath, bool isDirectory);
    virtual void didSucceed();

private:
    EntryCallbacks(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, PassRefPtr<DOMFileSystemBase>, const String& expectedPath, bool isDirectory);
    RefPtr<EntryCallback> m_successCallback;
    String m_expectedPath;
    bool m_isDirectory;
};

class EntriesCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, PassRefPtr<DirectoryReaderBase>, const String& basePath);
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory);
    virtual void didReadDirectoryEntries(bool hasMore);

private:
    EntriesCallbacks(PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, PassRefPtr<DirectoryReaderBase>, const String& basePath);
    RefPtr<EntriesCallback> m_successCallback;
    RefPtr<DirectoryReaderBase> m_directoryReader;
    String m_basePath;
    EntryVector m_entries;
};

class FileSystemCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<FileSystemCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*, FileSystemType);
    virtual void didOpenFileSystem(const String& name, const KURL& rootURL);

private:
    FileSystemCallbacks(PassRefPtr<FileSystemCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*, FileSystemType);
    RefPtr<FileSystemCallback> m_successCallback;
    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    FileSystemType m_type;
};

class ResolveURICallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*, FileSystemType, const String& filePath);
    virtual void didOpenFileSystem(const String& name, const KURL& rootURL);
    virtual void didResolveURL(const String& name, const KURL& rootURL, FileSystemType, const String& filePath, bool isDirectory);

private:
    ResolveURICallbacks(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*, FileSystemType, const String& filePath);
    RefPtr<EntryCallback> m_successCallback;
    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    FileSystemType m_type;
    String m_filePath;
};

class MetadataCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>, DOMFileSystemBase*);
    virtual void didReadMetadata(const FileMetadata&);

private:
    MetadataCallbacks(PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>, DOMFileSystemBase*);
    RefPtr<MetadataCallback> m_successCallback;
};

class FileWriterBaseCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<FileWriterBase>, PassRefPtr<FileWriterBaseCallback>, PassRefPtr<ErrorCallback>);
    virtual void didCreateFileWriter(PassOwnPtr<WebKit::WebFileWriter>, long long length);

private:
    FileWriterBaseCallbacks(PassRefPtr<FileWriterBase>, PassRefPtr<FileWriterBaseCallback>, PassRefPtr<ErrorCallback>);
    RefPtr<FileWriterBase> m_fileWriter;
    RefPtr<FileWriterBaseCallback> m_successCallback;
};

class VoidCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>, DOMFileSystemBase*);
    virtual void didSucceed();

private:
    VoidCallbacks(PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>, DOMFileSystemBase*);
    RefPtr<VoidCallback> m_successCallback;
};

} // namespace

#endif // FileSystemCallbacks_h
