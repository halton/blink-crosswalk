/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/ScriptEventListener.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8WindowShell.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/EventListener.h"
#include "core/page/Frame.h"

namespace WebCore {

static const String& eventParameterName(bool isSVGEvent)
{
    DEFINE_STATIC_LOCAL(const String, eventString, ("event"));
    DEFINE_STATIC_LOCAL(const String, evtString, ("evt"));
    return isSVGEvent ? evtString : eventString;
}

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Node* node, const QualifiedName& name, const AtomicString& value)
{
    ASSERT(node);
    if (value.isNull())
        return 0;

    // FIXME: Very strange: we initialize zero-based number with '1'.
    TextPosition position(OrdinalNumber::fromZeroBasedInt(1), OrdinalNumber::first());
    String sourceURL;

    v8::Isolate* isolate;
    if (Frame* frame = node->document().frame()) {
        isolate = toIsolate(frame);
        ScriptController* scriptController = frame->script();
        if (!scriptController->canExecuteScripts(AboutToExecuteScript))
            return 0;
        position = scriptController->eventHandlerPosition();
        sourceURL = node->document().url().string();
    } else {
        isolate = v8::Isolate::GetCurrent();
    }

    return V8LazyEventListener::create(name.localName().string(), eventParameterName(node->isSVGElement()), value, sourceURL, position, node, isolate);
}

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Frame* frame, const QualifiedName& name, const AtomicString& value)
{
    if (!frame)
        return 0;

    if (value.isNull())
        return 0;

    ScriptController* scriptController = frame->script();
    if (!scriptController->canExecuteScripts(AboutToExecuteScript))
        return 0;

    TextPosition position = scriptController->eventHandlerPosition();
    String sourceURL = frame->document()->url().string();

    return V8LazyEventListener::create(name.localName().string(), eventParameterName(frame->document()->isSVGDocument()), value, sourceURL, position, 0, toIsolate(frame));
}

String eventListenerHandlerBody(Document* document, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return "";

    v8::HandleScope scope(toIsolate(document));
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Context> context = toV8Context(document, v8Listener->world());
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::Value> function = v8Listener->getListenerObject(document);
    if (function.IsEmpty())
        return "";

    return toWebCoreStringWithNullCheck(function);
}

ScriptValue eventListenerHandler(Document* document, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return ScriptValue();

    v8::Isolate* isolate = toIsolate(document);
    v8::HandleScope scope(isolate);
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Context> context = toV8Context(document, v8Listener->world());
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::Object> function = v8Listener->getListenerObject(document);
    if (function.IsEmpty())
        return ScriptValue();
    return ScriptValue(function, isolate);
}

ScriptState* eventListenerHandlerScriptState(Frame* frame, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return 0;
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::HandleScope scope(toIsolate(frame));
    v8::Handle<v8::Context> v8Context = frame->script()->windowShell(v8Listener->world())->context();
    return ScriptState::forContext(v8Context);
}

bool eventListenerHandlerLocation(Document* document, EventListener* listener, String& sourceName, String& scriptId, int& lineNumber)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return false;

    v8::HandleScope scope(toIsolate(document));
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Context> context = toV8Context(document, v8Listener->world());
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::Object> object = v8Listener->getListenerObject(document);
    if (object.IsEmpty() || !object->IsFunction())
        return false;

    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(object);
    v8::Handle<v8::Value> scriptIdValue = function->GetScriptId();
    scriptId = toWebCoreStringWithUndefinedOrNullCheck(scriptIdValue);
    v8::ScriptOrigin origin = function->GetScriptOrigin();
    if (!origin.ResourceName().IsEmpty() && origin.ResourceName()->IsString())
        sourceName = toWebCoreString(origin.ResourceName().As<v8::String>());
    else
        sourceName = "";
    lineNumber = function->GetScriptLineNumber();
    return true;
}

} // namespace WebCore
