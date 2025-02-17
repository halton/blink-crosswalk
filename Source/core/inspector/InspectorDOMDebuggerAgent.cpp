/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/inspector/InspectorDOMDebuggerAgent.h"

#include "InspectorFrontend.h"
#include "core/dom/Event.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/platform/JSONValues.h"
#include "wtf/text/WTFString.h"

namespace {

enum DOMBreakpointType {
    SubtreeModified = 0,
    AttributeModified,
    NodeRemoved,
    DOMBreakpointTypesCount
};

static const char* const listenerEventCategoryType = "listener:";
static const char* const instrumentationEventCategoryType = "instrumentation:";

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

}

namespace WebCore {

static const char* const requestAnimationFrameEventName = "requestAnimationFrame";
static const char* const cancelAnimationFrameEventName = "cancelAnimationFrame";
static const char* const animationFrameFiredEventName = "animationFrameFired";
static const char* const setTimerEventName = "setTimer";
static const char* const clearTimerEventName = "clearTimer";
static const char* const timerFiredEventName = "timerFired";
static const char* const webglErrorFiredEventName = "webglErrorFired";
static const char* const webglWarningFiredEventName = "webglWarningFired";
static const char* const webglErrorNameProperty = "webglErrorName";

namespace DOMDebuggerAgentState {
static const char eventListenerBreakpoints[] = "eventListenerBreakpoints";
static const char pauseOnAllXHRs[] = "pauseOnAllXHRs";
static const char xhrBreakpoints[] = "xhrBreakpoints";
}

PassOwnPtr<InspectorDOMDebuggerAgent> InspectorDOMDebuggerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* inspectorState, InspectorDOMAgent* domAgent, InspectorDebuggerAgent* debuggerAgent)
{
    return adoptPtr(new InspectorDOMDebuggerAgent(instrumentingAgents, inspectorState, domAgent, debuggerAgent));
}

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* inspectorState, InspectorDOMAgent* domAgent, InspectorDebuggerAgent* debuggerAgent)
    : InspectorBaseAgent<InspectorDOMDebuggerAgent>("DOMDebugger", instrumentingAgents, inspectorState)
    , m_domAgent(domAgent)
    , m_debuggerAgent(debuggerAgent)
    , m_pauseInNextEventListener(false)
{
    m_debuggerAgent->setListener(this);
}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent()
{
    ASSERT(!m_debuggerAgent);
    ASSERT(!m_instrumentingAgents->inspectorDOMDebuggerAgent());
}

// Browser debugger agent enabled only when JS debugger is enabled.
void InspectorDOMDebuggerAgent::debuggerWasEnabled()
{
    m_instrumentingAgents->setInspectorDOMDebuggerAgent(this);
}

void InspectorDOMDebuggerAgent::debuggerWasDisabled()
{
    disable();
}

void InspectorDOMDebuggerAgent::stepInto()
{
    m_pauseInNextEventListener = true;
}

void InspectorDOMDebuggerAgent::didPause()
{
    m_pauseInNextEventListener = false;
}

void InspectorDOMDebuggerAgent::didProcessTask()
{
    if (!m_pauseInNextEventListener)
        return;
    if (m_debuggerAgent && m_debuggerAgent->runningNestedMessageLoop())
        return;
    m_pauseInNextEventListener = false;
}

void InspectorDOMDebuggerAgent::disable()
{
    m_instrumentingAgents->setInspectorDOMDebuggerAgent(0);
    clear();
}

void InspectorDOMDebuggerAgent::clearFrontend()
{
    disable();
}

void InspectorDOMDebuggerAgent::discardAgent()
{
    m_debuggerAgent->setListener(0);
    m_debuggerAgent = 0;
}

void InspectorDOMDebuggerAgent::discardBindings()
{
    m_domBreakpoints.clear();
}

void InspectorDOMDebuggerAgent::setEventListenerBreakpoint(ErrorString* error, const String& eventName)
{
    setBreakpoint(error, String(listenerEventCategoryType) + eventName);
}

void InspectorDOMDebuggerAgent::setInstrumentationBreakpoint(ErrorString* error, const String& eventName)
{
    setBreakpoint(error, String(instrumentationEventCategoryType) + eventName);
}

void InspectorDOMDebuggerAgent::setBreakpoint(ErrorString* error, const String& eventName)
{
    if (eventName.isEmpty()) {
        *error = "Event name is empty";
        return;
    }

    RefPtr<JSONObject> eventListenerBreakpoints = m_state->getObject(DOMDebuggerAgentState::eventListenerBreakpoints);
    eventListenerBreakpoints->setBoolean(eventName, true);
    m_state->setObject(DOMDebuggerAgentState::eventListenerBreakpoints, eventListenerBreakpoints);
}

void InspectorDOMDebuggerAgent::removeEventListenerBreakpoint(ErrorString* error, const String& eventName)
{
    removeBreakpoint(error, String(listenerEventCategoryType) + eventName);
}

void InspectorDOMDebuggerAgent::removeInstrumentationBreakpoint(ErrorString* error, const String& eventName)
{
    removeBreakpoint(error, String(instrumentationEventCategoryType) + eventName);
}

void InspectorDOMDebuggerAgent::removeBreakpoint(ErrorString* error, const String& eventName)
{
    if (eventName.isEmpty()) {
        *error = "Event name is empty";
        return;
    }

    RefPtr<JSONObject> eventListenerBreakpoints = m_state->getObject(DOMDebuggerAgentState::eventListenerBreakpoints);
    eventListenerBreakpoints->remove(eventName);
    m_state->setObject(DOMDebuggerAgentState::eventListenerBreakpoints, eventListenerBreakpoints);
}

void InspectorDOMDebuggerAgent::didInvalidateStyleAttr(Node* node)
{
    if (hasBreakpoint(node, AttributeModified)) {
        RefPtr<JSONObject> eventData = JSONObject::create();
        descriptionForDOMEvent(node, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::didInsertDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        uint32_t mask = m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(node));
        uint32_t inheritableTypesMask = (mask | (mask >> domBreakpointDerivedTypeShift)) & inheritableDOMBreakpointTypesMask;
        if (inheritableTypesMask)
            updateSubtreeBreakpoints(node, inheritableTypesMask, true);
    }
}

void InspectorDOMDebuggerAgent::didRemoveDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        // Remove subtree breakpoints.
        m_domBreakpoints.remove(node);
        Vector<Node*> stack(1, InspectorDOMAgent::innerFirstChild(node));
        do {
            Node* node = stack.last();
            stack.removeLast();
            if (!node)
                continue;
            m_domBreakpoints.remove(node);
            stack.append(InspectorDOMAgent::innerFirstChild(node));
            stack.append(InspectorDOMAgent::innerNextSibling(node));
        } while (!stack.isEmpty());
    }
}

static int domTypeForName(ErrorString* errorString, const String& typeString)
{
    if (typeString == "subtree-modified")
        return SubtreeModified;
    if (typeString == "attribute-modified")
        return AttributeModified;
    if (typeString == "node-removed")
        return NodeRemoved;
    *errorString = "Unknown DOM breakpoint type: " + typeString;
    return -1;
}

static String domTypeName(int type)
{
    switch (type) {
    case SubtreeModified: return "subtree-modified";
    case AttributeModified: return "attribute-modified";
    case NodeRemoved: return "node-removed";
    default: break;
    }
    return "";
}

void InspectorDOMDebuggerAgent::setDOMBreakpoint(ErrorString* errorString, int nodeId, const String& typeString)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    m_domBreakpoints.set(node, m_domBreakpoints.get(node) | rootBit);
    if (rootBit & inheritableDOMBreakpointTypesMask) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, true);
    }
}

void InspectorDOMDebuggerAgent::removeDOMBreakpoint(ErrorString* errorString, int nodeId, const String& typeString)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;
    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    uint32_t mask = m_domBreakpoints.get(node) & ~rootBit;
    if (mask)
        m_domBreakpoints.set(node, mask);
    else
        m_domBreakpoints.remove(node);

    if ((rootBit & inheritableDOMBreakpointTypesMask) && !(mask & (rootBit << domBreakpointDerivedTypeShift))) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, false);
    }
}

void InspectorDOMDebuggerAgent::willInsertDOMNode(Node* parent)
{
    if (hasBreakpoint(parent, SubtreeModified)) {
        RefPtr<JSONObject> eventData = JSONObject::create();
        descriptionForDOMEvent(parent, SubtreeModified, true, eventData.get());
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::willRemoveDOMNode(Node* node)
{
    Node* parentNode = InspectorDOMAgent::innerParentNode(node);
    if (hasBreakpoint(node, NodeRemoved)) {
        RefPtr<JSONObject> eventData = JSONObject::create();
        descriptionForDOMEvent(node, NodeRemoved, false, eventData.get());
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::DOM, eventData.release());
    } else if (parentNode && hasBreakpoint(parentNode, SubtreeModified)) {
        RefPtr<JSONObject> eventData = JSONObject::create();
        descriptionForDOMEvent(node, SubtreeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::DOM, eventData.release());
    }
    didRemoveDOMNode(node);
}

void InspectorDOMDebuggerAgent::willModifyDOMAttr(Element* element, const AtomicString&, const AtomicString&)
{
    if (hasBreakpoint(element, AttributeModified)) {
        RefPtr<JSONObject> eventData = JSONObject::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, JSONObject* description)
{
    ASSERT(hasBreakpoint(target, breakpointType));

    Node* breakpointOwner = target;
    if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
        // For inheritable breakpoint types, target node isn't always the same as the node that owns a breakpoint.
        // Target node may be unknown to frontend, so we need to push it first.
        RefPtr<TypeBuilder::Runtime::RemoteObject> targetNodeObject = m_domAgent->resolveNode(target, InspectorDebuggerAgent::backtraceObjectGroup);
        description->setValue("targetNode", targetNodeObject);

        // Find breakpoint owner node.
        if (!insertion)
            breakpointOwner = InspectorDOMAgent::innerParentNode(target);
        ASSERT(breakpointOwner);
        while (!(m_domBreakpoints.get(breakpointOwner) & (1 << breakpointType))) {
            Node* parentNode = InspectorDOMAgent::innerParentNode(breakpointOwner);
            if (!parentNode)
                break;
            breakpointOwner = parentNode;
        }

        if (breakpointType == SubtreeModified)
            description->setBoolean("insertion", insertion);
    }

    int breakpointOwnerNodeId = m_domAgent->boundNodeId(breakpointOwner);
    ASSERT(breakpointOwnerNodeId);
    description->setNumber("nodeId", breakpointOwnerNodeId);
    description->setString("type", domTypeName(breakpointType));
}

bool InspectorDOMDebuggerAgent::hasBreakpoint(Node* node, int type)
{
    uint32_t rootBit = 1 << type;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

void InspectorDOMDebuggerAgent::updateSubtreeBreakpoints(Node* node, uint32_t rootMask, bool set)
{
    uint32_t oldMask = m_domBreakpoints.get(node);
    uint32_t derivedMask = rootMask << domBreakpointDerivedTypeShift;
    uint32_t newMask = set ? oldMask | derivedMask : oldMask & ~derivedMask;
    if (newMask)
        m_domBreakpoints.set(node, newMask);
    else
        m_domBreakpoints.remove(node);

    uint32_t newRootMask = rootMask & ~newMask;
    if (!newRootMask)
        return;

    for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
        updateSubtreeBreakpoints(child, newRootMask, set);
}

void InspectorDOMDebuggerAgent::pauseOnNativeEventIfNeeded(PassRefPtr<JSONObject> eventData, bool synchronous)
{
    if (!eventData)
        return;
    if (synchronous)
        m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::EventListener, eventData);
    else
        m_debuggerAgent->schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::EventListener, eventData);
}

PassRefPtr<JSONObject> InspectorDOMDebuggerAgent::preparePauseOnNativeEventData(bool isDOMEvent, const String& eventName)
{
    String fullEventName = (isDOMEvent ? listenerEventCategoryType : instrumentationEventCategoryType) + eventName;
    if (m_pauseInNextEventListener)
        m_pauseInNextEventListener = false;
    else {
        RefPtr<JSONObject> eventListenerBreakpoints = m_state->getObject(DOMDebuggerAgentState::eventListenerBreakpoints);
        if (eventListenerBreakpoints->find(fullEventName) == eventListenerBreakpoints->end())
            return 0;
    }

    RefPtr<JSONObject> eventData = JSONObject::create();
    eventData->setString("eventName", fullEventName);
    return eventData.release();
}

void InspectorDOMDebuggerAgent::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, setTimerEventName), true);
}

void InspectorDOMDebuggerAgent::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, clearTimerEventName), true);
}

void InspectorDOMDebuggerAgent::willFireTimer(ScriptExecutionContext* context, int timerId)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, timerFiredEventName), false);
}

void InspectorDOMDebuggerAgent::didRequestAnimationFrame(Document* document, int callbackId)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, requestAnimationFrameEventName), true);
}

void InspectorDOMDebuggerAgent::didCancelAnimationFrame(Document* document, int callbackId)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, cancelAnimationFrameEventName), true);
}

void InspectorDOMDebuggerAgent::willFireAnimationFrame(Document* document, int callbackId)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, animationFrameFiredEventName), false);
}

void InspectorDOMDebuggerAgent::willHandleEvent(Event* event)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(true, event->type()), false);
}

void InspectorDOMDebuggerAgent::didFireWebGLError(const String& errorName)
{
    RefPtr<JSONObject> eventData = preparePauseOnNativeEventData(false, webglErrorFiredEventName);
    if (!eventData)
        return;
    if (!errorName.isEmpty())
        eventData->setString(webglErrorNameProperty, errorName);
    pauseOnNativeEventIfNeeded(eventData.release(), m_debuggerAgent->canBreakProgram());
}

void InspectorDOMDebuggerAgent::didFireWebGLWarning()
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(false, webglWarningFiredEventName), m_debuggerAgent->canBreakProgram());
}

void InspectorDOMDebuggerAgent::didFireWebGLErrorOrWarning(const String& message)
{
    if (message.findIgnoringCase("error") != WTF::kNotFound)
        didFireWebGLError(String());
    else
        didFireWebGLWarning();
}

void InspectorDOMDebuggerAgent::setXHRBreakpoint(ErrorString*, const String& url)
{
    if (url.isEmpty()) {
        m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, true);
        return;
    }

    RefPtr<JSONObject> xhrBreakpoints = m_state->getObject(DOMDebuggerAgentState::xhrBreakpoints);
    xhrBreakpoints->setBoolean(url, true);
    m_state->setObject(DOMDebuggerAgentState::xhrBreakpoints, xhrBreakpoints);
}

void InspectorDOMDebuggerAgent::removeXHRBreakpoint(ErrorString*, const String& url)
{
    if (url.isEmpty()) {
        m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, false);
        return;
    }

    RefPtr<JSONObject> xhrBreakpoints = m_state->getObject(DOMDebuggerAgentState::xhrBreakpoints);
    xhrBreakpoints->remove(url);
    m_state->setObject(DOMDebuggerAgentState::xhrBreakpoints, xhrBreakpoints);
}

void InspectorDOMDebuggerAgent::willSendXMLHttpRequest(const String& url)
{
    String breakpointURL;
    if (m_state->getBoolean(DOMDebuggerAgentState::pauseOnAllXHRs))
        breakpointURL = "";
    else {
        RefPtr<JSONObject> xhrBreakpoints = m_state->getObject(DOMDebuggerAgentState::xhrBreakpoints);
        for (JSONObject::iterator it = xhrBreakpoints->begin(); it != xhrBreakpoints->end(); ++it) {
            if (url.contains(it->key)) {
                breakpointURL = it->key;
                break;
            }
        }
    }

    if (breakpointURL.isNull())
        return;

    RefPtr<JSONObject> eventData = JSONObject::create();
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    m_debuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::XHR, eventData.release());
}

void InspectorDOMDebuggerAgent::clear()
{
    m_domBreakpoints.clear();
    m_pauseInNextEventListener = false;
}

} // namespace WebCore

