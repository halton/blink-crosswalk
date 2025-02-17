/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "config.h"
#include "core/fileapi/FileReader.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ProgressEvent.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/File.h"
#include "core/platform/Logging.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CString.h"

namespace WebCore {

namespace {

const CString utf8BlobURL(Blob* blob)
{
    return blob->url().string().utf8();
}

const CString utf8FilePath(Blob* blob)
{
    return blob->isFile() ? toFile(blob)->path().utf8() : "";
}

} // namespace

static const double progressNotificationIntervalMS = 50;

PassRefPtr<FileReader> FileReader::create(ScriptExecutionContext* context)
{
    RefPtr<FileReader> fileReader(adoptRef(new FileReader(context)));
    fileReader->suspendIfNeeded();
    return fileReader.release();
}

FileReader::FileReader(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_state(EMPTY)
    , m_loadingState(LoadingStateNone)
    , m_readType(FileReaderLoader::ReadAsBinaryString)
    , m_lastProgressNotificationTimeMS(0)
{
    ScriptWrappable::init(this);
}

FileReader::~FileReader()
{
    terminate();
}

const AtomicString& FileReader::interfaceName() const
{
    return eventNames().interfaceForFileReader;
}

bool FileReader::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a FileReader, so pages with FileReader can not go into page cache.
    return false;
}

void FileReader::stop()
{
    terminate();
}

void FileReader::readAsArrayBuffer(Blob* blob, ExceptionState& es)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as array buffer: %s %s\n", utf8BlobURL(blob).data(), utf8FilePath(blob).data());

    readInternal(blob, FileReaderLoader::ReadAsArrayBuffer, es);
}

void FileReader::readAsBinaryString(Blob* blob, ExceptionState& es)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as binary: %s %s\n", utf8BlobURL(blob).data(), utf8FilePath(blob).data());

    readInternal(blob, FileReaderLoader::ReadAsBinaryString, es);
}

void FileReader::readAsText(Blob* blob, const String& encoding, ExceptionState& es)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as text: %s %s\n", utf8BlobURL(blob).data(), utf8FilePath(blob).data());

    m_encoding = encoding;
    readInternal(blob, FileReaderLoader::ReadAsText, es);
}

void FileReader::readAsText(Blob* blob, ExceptionState& es)
{
    readAsText(blob, String(), es);
}

void FileReader::readAsDataURL(Blob* blob, ExceptionState& es)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as data URL: %s %s\n", utf8BlobURL(blob).data(), utf8FilePath(blob).data());

    readInternal(blob, FileReaderLoader::ReadAsDataURL, es);
}

void FileReader::readInternal(Blob* blob, FileReaderLoader::ReadType type, ExceptionState& es)
{
    // If multiple concurrent read methods are called on the same FileReader, InvalidStateError should be thrown when the state is LOADING.
    if (m_state == LOADING) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    setPendingActivity(this);

    m_blob = blob;
    m_readType = type;
    m_state = LOADING;
    m_loadingState = LoadingStateLoading;
    m_error = 0;

    m_loader = adoptPtr(new FileReaderLoader(m_readType, this));
    m_loader->setEncoding(m_encoding);
    m_loader->setDataType(m_blob->type());
    m_loader->start(scriptExecutionContext(), *m_blob);
}

static void delayedAbort(ScriptExecutionContext*, FileReader* reader)
{
    reader->doAbort();
}

void FileReader::abort()
{
    LOG(FileAPI, "FileReader: aborting\n");

    if (m_loadingState != LoadingStateLoading)
        return;
    m_loadingState = LoadingStateAborted;

    // Schedule to have the abort done later since abort() might be called from the event handler and we do not want the resource loading code to be in the stack.
    scriptExecutionContext()->postTask(
        createCallbackTask(&delayedAbort, AllowAccessLater(this)));
}

void FileReader::doAbort()
{
    ASSERT(m_state != DONE);

    terminate();

    m_error = FileError::create(FileError::ABORT_ERR);

    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().abortEvent);
    fireEvent(eventNames().loadendEvent);

    // All possible events have fired and we're done, no more pending activity.
    unsetPendingActivity(this);
}

void FileReader::terminate()
{
    if (m_loader) {
        m_loader->cancel();
        m_loader = nullptr;
    }
    m_state = DONE;
    m_loadingState = LoadingStateNone;
}

void FileReader::didStartLoading()
{
    fireEvent(eventNames().loadstartEvent);
}

void FileReader::didReceiveData()
{
    // Fire the progress event at least every 50ms.
    double now = currentTimeMS();
    if (!m_lastProgressNotificationTimeMS)
        m_lastProgressNotificationTimeMS = now;
    else if (now - m_lastProgressNotificationTimeMS > progressNotificationIntervalMS) {
        fireEvent(eventNames().progressEvent);
        m_lastProgressNotificationTimeMS = now;
    }
}

void FileReader::didFinishLoading()
{
    if (m_loadingState == LoadingStateAborted)
        return;
    ASSERT(m_loadingState == LoadingStateLoading);

    // It's important that we change m_loadingState before firing any events
    // since any of the events could call abort(), which internally checks
    // if we're still loading (therefore we need abort process) or not.
    m_loadingState = LoadingStateNone;

    fireEvent(eventNames().progressEvent);

    ASSERT(m_state != DONE);
    m_state = DONE;

    fireEvent(eventNames().loadEvent);
    fireEvent(eventNames().loadendEvent);

    // All possible events have fired and we're done, no more pending activity.
    unsetPendingActivity(this);
}

void FileReader::didFail(FileError::ErrorCode errorCode)
{
    if (m_loadingState == LoadingStateAborted)
        return;
    ASSERT(m_loadingState == LoadingStateLoading);
    m_loadingState = LoadingStateNone;

    ASSERT(m_state != DONE);
    m_state = DONE;

    m_error = FileError::create(static_cast<FileError::ErrorCode>(errorCode));
    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().loadendEvent);

    // All possible events have fired and we're done, no more pending activity.
    unsetPendingActivity(this);
}

void FileReader::fireEvent(const AtomicString& type)
{
    dispatchEvent(ProgressEvent::create(type, true, m_loader ? m_loader->bytesLoaded() : 0, m_loader ? m_loader->totalBytes() : 0));
}

PassRefPtr<ArrayBuffer> FileReader::arrayBufferResult() const
{
    if (!m_loader || m_error)
        return 0;
    return m_loader->arrayBufferResult();
}

String FileReader::stringResult()
{
    if (!m_loader || m_error)
        return String();
    return m_loader->stringResult();
}

} // namespace WebCore
