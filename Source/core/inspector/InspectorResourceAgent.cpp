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
#include "core/inspector/InspectorResourceAgent.h"

#include "FetchInitiatorTypeNames.h"
#include "InspectorFrontend.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/NetworkResourcesData.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/JSONValues.h"
#include "core/platform/network/HTTPHeaderMap.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/xml/XMLHttpRequest.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandshakeRequest.h"
#include "modules/websockets/WebSocketHandshakeResponse.h"
#include "weborigin/KURL.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"

typedef WebCore::InspectorBackendDispatcher::NetworkCommandHandler::LoadResourceForFrontendCallback LoadResourceForFrontendCallback;

namespace WebCore {

namespace ResourceAgentState {
static const char resourceAgentEnabled[] = "resourceAgentEnabled";
static const char extraRequestHeaders[] = "extraRequestHeaders";
static const char cacheDisabled[] = "cacheDisabled";
static const char userAgentOverride[] = "userAgentOverride";
}

namespace {

static PassRefPtr<JSONObject> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    RefPtr<JSONObject> headersObject = JSONObject::create();
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
        headersObject->setString(it->key.string(), it->value);
    return headersObject;
}

class InspectorThreadableLoaderClient : public ThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorThreadableLoaderClient);
public:
    InspectorThreadableLoaderClient(PassRefPtr<LoadResourceForFrontendCallback> callback)
        : m_callback(callback)
        , m_statusCode(0) { }

    virtual ~InspectorThreadableLoaderClient() { }

    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
    {
        WTF::TextEncoding textEncoding(response.textEncodingName());
        bool useDetector = false;
        if (!textEncoding.isValid()) {
            textEncoding = UTF8Encoding();
            useDetector = true;
        }
        m_decoder = TextResourceDecoder::create("text/plain", textEncoding, useDetector);
        m_statusCode = response.httpStatusCode();
        m_responseHeaders = response.httpHeaderFields();
    }

    virtual void didReceiveData(const char* data, int dataLength)
    {
        if (!dataLength)
            return;

        if (dataLength == -1)
            dataLength = strlen(data);

        m_responseText = m_responseText.concatenateWith(m_decoder->decode(data, dataLength));
    }

    virtual void didFinishLoading(unsigned long /*identifier*/, double /*finishTime*/)
    {
        if (m_decoder)
            m_responseText = m_responseText.concatenateWith(m_decoder->flush());
        m_callback->sendSuccess(m_statusCode, buildObjectForHeaders(m_responseHeaders), m_responseText.flattenToString());
        dispose();
    }

    virtual void didFail(const ResourceError&)
    {
        m_callback->sendFailure("Loading resource for inspector failed");
        dispose();
    }

    virtual void didFailRedirectCheck()
    {
        m_callback->sendFailure("Loading resource for inspector failed redirect check");
        dispose();
    }

    void didFailLoaderCreation()
    {
        m_callback->sendFailure("Couldn't create a loader");
        dispose();
    }

    void setLoader(PassRefPtr<ThreadableLoader> loader)
    {
        m_loader = loader;
    }

private:
    void dispose()
    {
        m_loader = 0;
        delete this;
    }

    RefPtr<LoadResourceForFrontendCallback> m_callback;
    RefPtr<ThreadableLoader> m_loader;
    RefPtr<TextResourceDecoder> m_decoder;
    ScriptString m_responseText;
    int m_statusCode;
    HTTPHeaderMap m_responseHeaders;
};

KURL urlWithoutFragment(const KURL& url)
{
    KURL result = url;
    result.removeFragmentIdentifier();
    return result;
}

} // namespace

void InspectorResourceAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->network();
}

void InspectorResourceAgent::clearFrontend()
{
    m_frontend = 0;
    ErrorString error;
    disable(&error);
}

void InspectorResourceAgent::restore()
{
    if (m_state->getBoolean(ResourceAgentState::resourceAgentEnabled))
        enable();
}

static PassRefPtr<TypeBuilder::Network::ResourceTiming> buildObjectForTiming(const ResourceLoadTiming& timing, DocumentLoader* loader)
{
    return TypeBuilder::Network::ResourceTiming::create()
        .setRequestTime(loader->timing()->monotonicTimeToPseudoWallTime(timing.requestTime))
        .setProxyStart(timing.calculateMillisecondDelta(timing.proxyStart))
        .setProxyEnd(timing.calculateMillisecondDelta(timing.proxyEnd))
        .setDnsStart(timing.calculateMillisecondDelta(timing.dnsStart))
        .setDnsEnd(timing.calculateMillisecondDelta(timing.dnsEnd))
        .setConnectStart(timing.calculateMillisecondDelta(timing.connectStart))
        .setConnectEnd(timing.calculateMillisecondDelta(timing.connectEnd))
        .setSslStart(timing.calculateMillisecondDelta(timing.sslStart))
        .setSslEnd(timing.calculateMillisecondDelta(timing.sslEnd))
        .setSendStart(timing.calculateMillisecondDelta(timing.sendStart))
        .setSendEnd(timing.calculateMillisecondDelta(timing.sendEnd))
        .setReceiveHeadersEnd(timing.calculateMillisecondDelta(timing.receiveHeadersEnd))
        .release();
}

static PassRefPtr<TypeBuilder::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request)
{
    RefPtr<TypeBuilder::Network::Request> requestObject = TypeBuilder::Network::Request::create()
        .setUrl(urlWithoutFragment(request.url()).string())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()));
    if (request.httpBody() && !request.httpBody()->isEmpty()) {
        Vector<char> bytes;
        request.httpBody()->flatten(bytes);
        requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.data(), bytes.size()));
    }
    return requestObject;
}

static PassRefPtr<TypeBuilder::Network::Response> buildObjectForResourceResponse(const ResourceResponse& response, DocumentLoader* loader)
{
    if (response.isNull())
        return 0;


    double status;
    String statusText;
    if (response.resourceLoadInfo() && response.resourceLoadInfo()->httpStatusCode) {
        status = response.resourceLoadInfo()->httpStatusCode;
        statusText = response.resourceLoadInfo()->httpStatusText;
    } else {
        status = response.httpStatusCode();
        statusText = response.httpStatusText();
    }
    RefPtr<JSONObject> headers;
    if (response.resourceLoadInfo())
        headers = buildObjectForHeaders(response.resourceLoadInfo()->responseHeaders);
    else
        headers = buildObjectForHeaders(response.httpHeaderFields());

    RefPtr<TypeBuilder::Network::Response> responseObject = TypeBuilder::Network::Response::create()
        .setUrl(urlWithoutFragment(response.url()).string())
        .setStatus(status)
        .setStatusText(statusText)
        .setHeaders(headers)
        .setMimeType(response.mimeType())
        .setConnectionReused(response.connectionReused())
        .setConnectionId(response.connectionID());

    responseObject->setFromDiskCache(response.wasCached());
    if (response.resourceLoadTiming())
        responseObject->setTiming(buildObjectForTiming(*response.resourceLoadTiming(), loader));

    if (response.resourceLoadInfo()) {
        if (!response.resourceLoadInfo()->responseHeadersText.isEmpty())
            responseObject->setHeadersText(response.resourceLoadInfo()->responseHeadersText);

        responseObject->setRequestHeaders(buildObjectForHeaders(response.resourceLoadInfo()->requestHeaders));
        if (!response.resourceLoadInfo()->requestHeadersText.isEmpty())
            responseObject->setRequestHeadersText(response.resourceLoadInfo()->requestHeadersText);
    }

    return responseObject;
}

InspectorResourceAgent::~InspectorResourceAgent()
{
    if (m_state->getBoolean(ResourceAgentState::resourceAgentEnabled)) {
        ErrorString error;
        disable(&error);
    }
    ASSERT(!m_instrumentingAgents->inspectorResourceAgent());
}

void InspectorResourceAgent::willSendRequest(unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    if (initiatorInfo.name == FetchInitiatorTypeNames::inspector)
        return;
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->resourceCreated(requestId, m_pageAgent->loaderId(loader));

    RefPtr<JSONObject> headers = m_state->getObject(ResourceAgentState::extraRequestHeaders);

    if (headers) {
        JSONObject::const_iterator end = headers->end();
        for (JSONObject::const_iterator it = headers->begin(); it != end; ++it) {
            String value;
            if (it->value->asString(&value))
                request.setHTTPHeaderField(it->key, value);
        }
    }

    request.setReportLoadTiming(true);
    request.setReportRawHeaders(true);

    if (m_state->getBoolean(ResourceAgentState::cacheDisabled)) {
        request.setHTTPHeaderField("Pragma", "no-cache");
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "no-cache");
    }

    String frameId = m_pageAgent->frameId(loader->frame());

    RefPtr<TypeBuilder::Network::Initiator> initiatorObject = buildInitiatorObject(loader->frame() ? loader->frame()->document() : 0, initiatorInfo);
    if (initiatorInfo.name == FetchInitiatorTypeNames::document) {
        FrameNavigationInitiatorMap::iterator it = m_frameNavigationInitiatorMap.find(frameId);
        if (it != m_frameNavigationInitiatorMap.end())
            initiatorObject = it->value;
    }

    m_frontend->requestWillBeSent(requestId, frameId, m_pageAgent->loaderId(loader), urlWithoutFragment(loader->url()).string(), buildObjectForResourceRequest(request), currentTime(), initiatorObject, buildObjectForResourceResponse(redirectResponse, loader));
}

void InspectorResourceAgent::markResourceAsCached(unsigned long identifier)
{
    m_frontend->requestServedFromCache(IdentifiersFactory::requestId(identifier));
}

void InspectorResourceAgent::didReceiveResourceResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (!loader)
        return;

    String requestId = IdentifiersFactory::requestId(identifier);
    RefPtr<TypeBuilder::Network::Response> resourceResponse = buildObjectForResourceResponse(response, loader);

    bool isNotModified = response.httpStatusCode() == 304;

    Resource* cachedResource = 0;
    if (resourceLoader && !isNotModified)
        cachedResource = resourceLoader->cachedResource();
    if (!cachedResource || cachedResource->type() == Resource::MainResource)
        cachedResource = InspectorPageAgent::cachedResource(loader->frame(), response.url());

    if (cachedResource) {
        // Use mime type from cached resource in case the one in response is empty.
        if (resourceResponse && response.mimeType().isEmpty())
            resourceResponse->setString(TypeBuilder::Network::Response::MimeType, cachedResource->response().mimeType());
        m_resourcesData->addResource(requestId, cachedResource);
    }

    InspectorPageAgent::ResourceType type = cachedResource ? InspectorPageAgent::cachedResourceType(*cachedResource) : InspectorPageAgent::OtherResource;
    if (m_resourcesData->resourceType(requestId) == InspectorPageAgent::XHRResource)
        type = InspectorPageAgent::XHRResource;
    else if (m_resourcesData->resourceType(requestId) == InspectorPageAgent::ScriptResource)
        type = InspectorPageAgent::ScriptResource;
    else if (equalIgnoringFragmentIdentifier(response.url(), loader->url()) && !loader->isCommitted())
        type = InspectorPageAgent::DocumentResource;

    m_resourcesData->responseReceived(requestId, m_pageAgent->frameId(loader->frame()), response);
    m_resourcesData->setResourceType(requestId, type);
    m_frontend->responseReceived(requestId, m_pageAgent->frameId(loader->frame()), m_pageAgent->loaderId(loader), currentTime(), InspectorPageAgent::resourceTypeJson(type), resourceResponse);
    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveData from the network stack.
    if (isNotModified && cachedResource && cachedResource->encodedSize())
        didReceiveData(identifier, 0, cachedResource->encodedSize(), 0);
}

static bool isErrorStatusCode(int statusCode)
{
    return statusCode >= 400;
}

void InspectorResourceAgent::didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    if (data) {
        NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
        if (resourceData && (!resourceData->cachedResource() || resourceData->cachedResource()->dataBufferingPolicy() == DoNotBufferData || isErrorStatusCode(resourceData->httpStatusCode())))
            m_resourcesData->maybeAddResourceData(requestId, data, dataLength);
    }

    m_frontend->dataReceived(requestId, currentTime(), dataLength, encodedDataLength);
}

void InspectorResourceAgent::didFinishLoading(unsigned long identifier, DocumentLoader* loader, double monotonicFinishTime)
{
    double finishTime = 0.0;
    // FIXME: Expose all of the timing details to inspector and have it calculate finishTime.
    if (monotonicFinishTime)
        finishTime = loader->timing()->monotonicTimeToPseudoWallTime(monotonicFinishTime);

    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->maybeDecodeDataToContent(requestId);
    if (!finishTime)
        finishTime = currentTime();
    m_frontend->loadingFinished(requestId, finishTime);
}

void InspectorResourceAgent::didReceiveCORSRedirectResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Update the response and finish loading
    didReceiveResourceResponse(identifier, loader, response, resourceLoader);
    didFinishLoading(identifier, loader, 0);
}

void InspectorResourceAgent::didFailLoading(unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    bool canceled = error.isCancellation();
    m_frontend->loadingFailed(requestId, currentTime(), error.localizedDescription(), canceled ? &canceled : 0);
}

void InspectorResourceAgent::scriptImported(unsigned long identifier, const String& sourceString)
{
    m_resourcesData->setResourceContent(IdentifiersFactory::requestId(identifier), sourceString);
}

void InspectorResourceAgent::didReceiveScriptResponse(unsigned long identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::ScriptResource);
}

void InspectorResourceAgent::documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient* client)
{
    if (!client)
        return;

    PendingXHRReplayDataMap::iterator it = m_pendingXHRReplayData.find(client);
    if (it == m_pendingXHRReplayData.end())
        return;

    XHRReplayData* xhrReplayData = it->value.get();
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->setXHRReplayData(requestId, xhrReplayData);
}

void InspectorResourceAgent::willLoadXHR(ThreadableLoaderClient* client, const String& method, const KURL& url, bool async, PassRefPtr<FormData> formData, const HTTPHeaderMap& headers, bool includeCredentials)
{
    RefPtr<XHRReplayData> xhrReplayData = XHRReplayData::create(method, urlWithoutFragment(url), async, formData, includeCredentials);
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it!= end; ++it)
        xhrReplayData->addHeader(it->key, it->value);
    m_pendingXHRReplayData.set(client, xhrReplayData);
}

void InspectorResourceAgent::didFailXHRLoading(ThreadableLoaderClient* client)
{
    m_pendingXHRReplayData.remove(client);
}

void InspectorResourceAgent::didFinishXHRLoading(ThreadableLoaderClient* client, unsigned long identifier, ScriptString sourceString, const String&, const String&, unsigned)
{
    m_pendingXHRReplayData.remove(client);
}

void InspectorResourceAgent::didReceiveXHRResponse(unsigned long identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::XHRResource);
}

void InspectorResourceAgent::willDestroyResource(Resource* cachedResource)
{
    Vector<String> requestIds = m_resourcesData->removeResource(cachedResource);
    if (!requestIds.size())
        return;

    String content;
    bool base64Encoded;
    if (!InspectorPageAgent::cachedResourceContent(cachedResource, &content, &base64Encoded))
        return;
    Vector<String>::iterator end = requestIds.end();
    for (Vector<String>::iterator it = requestIds.begin(); it != end; ++it)
        m_resourcesData->setResourceContent(*it, content, base64Encoded);
}

void InspectorResourceAgent::applyUserAgentOverride(String* userAgent)
{
    String userAgentOverride = m_state->getString(ResourceAgentState::userAgentOverride);
    if (!userAgentOverride.isEmpty())
        *userAgent = userAgentOverride;
}

void InspectorResourceAgent::willRecalculateStyle(Document*)
{
    m_isRecalculatingStyle = true;
}

void InspectorResourceAgent::didRecalculateStyle()
{
    m_isRecalculatingStyle = false;
    m_styleRecalculationInitiator = nullptr;
}

void InspectorResourceAgent::didScheduleStyleRecalculation(Document* document)
{
    if (!m_styleRecalculationInitiator)
        m_styleRecalculationInitiator = buildInitiatorObject(document, FetchInitiatorInfo());
}

PassRefPtr<TypeBuilder::Network::Initiator> InspectorResourceAgent::buildInitiatorObject(Document* document, const FetchInitiatorInfo& initiatorInfo)
{
    RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);
    if (stackTrace && stackTrace->size() > 0) {
        RefPtr<TypeBuilder::Network::Initiator> initiatorObject = TypeBuilder::Network::Initiator::create()
            .setType(TypeBuilder::Network::Initiator::Type::Script);
        initiatorObject->setStackTrace(stackTrace->buildInspectorArray());
        return initiatorObject;
    }

    if (document && document->scriptableDocumentParser()) {
        RefPtr<TypeBuilder::Network::Initiator> initiatorObject = TypeBuilder::Network::Initiator::create()
            .setType(TypeBuilder::Network::Initiator::Type::Parser);
        initiatorObject->setUrl(urlWithoutFragment(document->url()).string());
        if (TextPosition::belowRangePosition() != initiatorInfo.position)
            initiatorObject->setLineNumber(initiatorInfo.position.m_line.oneBasedInt());
        else
            initiatorObject->setLineNumber(document->scriptableDocumentParser()->lineNumber().oneBasedInt());
        return initiatorObject;
    }

    if (m_isRecalculatingStyle && m_styleRecalculationInitiator)
        return m_styleRecalculationInitiator;

    return TypeBuilder::Network::Initiator::create()
        .setType(TypeBuilder::Network::Initiator::Type::Other)
        .release();
}

void InspectorResourceAgent::didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const String&)
{
    m_frontend->webSocketCreated(IdentifiersFactory::requestId(identifier), urlWithoutFragment(requestURL).string());
}

void InspectorResourceAgent::willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    RefPtr<TypeBuilder::Network::WebSocketRequest> requestObject = TypeBuilder::Network::WebSocketRequest::create()
        .setHeaders(buildObjectForHeaders(request.headerFields()));
    m_frontend->webSocketWillSendHandshakeRequest(IdentifiersFactory::requestId(identifier), currentTime(), requestObject);
}

void InspectorResourceAgent::didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    RefPtr<TypeBuilder::Network::WebSocketResponse> responseObject = TypeBuilder::Network::WebSocketResponse::create()
        .setStatus(response.statusCode())
        .setStatusText(response.statusText())
        .setHeaders(buildObjectForHeaders(response.headerFields()));
    m_frontend->webSocketHandshakeResponseReceived(IdentifiersFactory::requestId(identifier), currentTime(), responseObject);
}

void InspectorResourceAgent::didCloseWebSocket(Document*, unsigned long identifier)
{
    m_frontend->webSocketClosed(IdentifiersFactory::requestId(identifier), currentTime());
}

void InspectorResourceAgent::didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame& frame)
{
    RefPtr<TypeBuilder::Network::WebSocketFrame> frameObject = TypeBuilder::Network::WebSocketFrame::create()
        .setOpcode(frame.opCode)
        .setMask(frame.masked)
        .setPayloadData(String(frame.payload, frame.payloadLength));
    m_frontend->webSocketFrameReceived(IdentifiersFactory::requestId(identifier), currentTime(), frameObject);
}

void InspectorResourceAgent::didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame& frame)
{
    RefPtr<TypeBuilder::Network::WebSocketFrame> frameObject = TypeBuilder::Network::WebSocketFrame::create()
        .setOpcode(frame.opCode)
        .setMask(frame.masked)
        .setPayloadData(String(frame.payload, frame.payloadLength));
    m_frontend->webSocketFrameSent(IdentifiersFactory::requestId(identifier), currentTime(), frameObject);
}

void InspectorResourceAgent::didReceiveWebSocketFrameError(unsigned long identifier, const String& errorMessage)
{
    m_frontend->webSocketFrameError(IdentifiersFactory::requestId(identifier), currentTime(), errorMessage);
}

// called from Internals for layout test purposes.
void InspectorResourceAgent::setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize)
{
    m_resourcesData->setResourcesDataSizeLimits(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

void InspectorResourceAgent::enable(ErrorString*)
{
    enable();
}

void InspectorResourceAgent::enable()
{
    if (!m_frontend)
        return;
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, true);
    m_instrumentingAgents->setInspectorResourceAgent(this);
}

void InspectorResourceAgent::disable(ErrorString*)
{
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, false);
    m_state->setString(ResourceAgentState::userAgentOverride, "");
    m_instrumentingAgents->setInspectorResourceAgent(0);
    m_resourcesData->clear();
}

void InspectorResourceAgent::setUserAgentOverride(ErrorString*, const String& userAgent)
{
    m_state->setString(ResourceAgentState::userAgentOverride, userAgent);
    m_overlay->setOverride(InspectorOverlay::UserAgentOverride, !userAgent.isEmpty());
}

void InspectorResourceAgent::setExtraHTTPHeaders(ErrorString*, const RefPtr<JSONObject>& headers)
{
    m_state->setObject(ResourceAgentState::extraRequestHeaders, headers);
}

void InspectorResourceAgent::getResponseBody(ErrorString* errorString, const String& requestId, String* content, bool* base64Encoded)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        *errorString = "No resource with given identifier found";
        return;
    }

    if (resourceData->hasContent()) {
        *base64Encoded = resourceData->base64Encoded();
        *content = resourceData->content();
        return;
    }

    if (resourceData->isContentEvicted()) {
        *errorString = "Request content was evicted from inspector cache";
        return;
    }

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        *base64Encoded = false;
        if (InspectorPageAgent::sharedBufferContent(resourceData->buffer(), resourceData->textEncodingName(), *base64Encoded, content))
            return;
    }

    if (resourceData->cachedResource()) {
        if (InspectorPageAgent::cachedResourceContent(resourceData->cachedResource(), content, base64Encoded))
            return;
    }

    *errorString = "No data found for resource with given identifier";
}

void InspectorResourceAgent::replayXHR(ErrorString*, const String& requestId)
{
    RefPtr<XMLHttpRequest> xhr = XMLHttpRequest::create(m_pageAgent->mainFrame()->document());
    String actualRequestId = requestId;

    XHRReplayData* xhrReplayData = m_resourcesData->xhrReplayData(requestId);
    if (!xhrReplayData)
        return;

    Resource* cachedResource = memoryCache()->resourceForURL(xhrReplayData->url());
    if (cachedResource)
        memoryCache()->remove(cachedResource);

    xhr->open(xhrReplayData->method(), xhrReplayData->url(), xhrReplayData->async(), IGNORE_EXCEPTION);
    HTTPHeaderMap::const_iterator end = xhrReplayData->headers().end();
    for (HTTPHeaderMap::const_iterator it = xhrReplayData->headers().begin(); it!= end; ++it)
        xhr->setRequestHeader(it->key, it->value, IGNORE_EXCEPTION);
    xhr->sendForInspectorXHRReplay(xhrReplayData->formData(), IGNORE_EXCEPTION);
}

void InspectorResourceAgent::canClearBrowserCache(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorResourceAgent::clearBrowserCache(ErrorString*)
{
    m_client->clearBrowserCache();
}

void InspectorResourceAgent::canClearBrowserCookies(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorResourceAgent::clearBrowserCookies(ErrorString*)
{
    m_client->clearBrowserCookies();
}

void InspectorResourceAgent::setCacheDisabled(ErrorString*, bool cacheDisabled)
{
    m_state->setBoolean(ResourceAgentState::cacheDisabled, cacheDisabled);
    if (cacheDisabled)
        memoryCache()->evictResources();
}

void InspectorResourceAgent::loadResourceForFrontend(ErrorString* errorString, const String& frameId, const String& url, const RefPtr<JSONObject>* requestHeaders, PassRefPtr<LoadResourceForFrontendCallback> prpCallback)
{
    RefPtr<LoadResourceForFrontendCallback> callback = prpCallback;
    Frame* frame = m_pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return;

    Document* document = frame->document();
    if (!document) {
        *errorString = "No Document instance for the specified frame";
        return;
    }

    ResourceRequest request(url);
    request.setHTTPMethod("GET");
    request.setCachePolicy(ReloadIgnoringCacheData);
    if (requestHeaders) {
        for (JSONObject::iterator it = (*requestHeaders)->begin(); it != (*requestHeaders)->end(); ++it) {
            String value;
            bool success = it->value->asString(&value);
            if (!success) {
                *errorString = "Request header \"" + it->key + "\" value is not a string";
                return;
            }
            request.addHTTPHeaderField(it->key, value);
        }
    }

    ThreadableLoaderOptions options;
    options.allowCredentials = AllowStoredCredentials;
    options.crossOriginRequestPolicy = AllowCrossOriginRequests;
    options.sendLoadCallbacks = SendCallbacks;

    InspectorThreadableLoaderClient* inspectorThreadableLoaderClient = new InspectorThreadableLoaderClient(callback);
    RefPtr<DocumentThreadableLoader> loader = DocumentThreadableLoader::create(document, inspectorThreadableLoaderClient, request, options);
    if (!loader) {
        inspectorThreadableLoaderClient->didFailLoaderCreation();
        return;
    }
    loader->setDefersLoading(false);
    if (!callback->isActive())
        return;
    inspectorThreadableLoaderClient->setLoader(loader.release());
}

void InspectorResourceAgent::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
    if (loader->frame() != frame->page()->mainFrame())
        return;

    if (m_state->getBoolean(ResourceAgentState::cacheDisabled))
        memoryCache()->evictResources();

    m_resourcesData->clear(m_pageAgent->loaderId(loader));
}

void InspectorResourceAgent::frameScheduledNavigation(Frame* frame, double)
{
    RefPtr<TypeBuilder::Network::Initiator> initiator = buildInitiatorObject(frame->document(), FetchInitiatorInfo());
    m_frameNavigationInitiatorMap.set(m_pageAgent->frameId(frame), initiator);
}

void InspectorResourceAgent::frameClearedScheduledNavigation(Frame* frame)
{
    m_frameNavigationInitiatorMap.remove(m_pageAgent->frameId(frame));
}

bool InspectorResourceAgent::fetchResourceContent(Frame* frame, const KURL& url, String* content, bool* base64Encoded)
{
    // First try to fetch content from the cached resource.
    Resource* cachedResource = frame->document()->fetcher()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url);
    if (cachedResource && InspectorPageAgent::cachedResourceContent(cachedResource, content, base64Encoded))
        return true;

    // Then fall back to resource data.
    Vector<NetworkResourcesData::ResourceData*> resources = m_resourcesData->resources();
    for (Vector<NetworkResourcesData::ResourceData*>::iterator it = resources.begin(); it != resources.end(); ++it) {
        if ((*it)->url() == url) {
            *content = (*it)->content();
            *base64Encoded = (*it)->base64Encoded();
            return true;
        }
    }
    return false;
}

InspectorResourceAgent::InspectorResourceAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorClient* client, InspectorCompositeState* state, InspectorOverlay* overlay)
    : InspectorBaseAgent<InspectorResourceAgent>("Network", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_client(client)
    , m_overlay(overlay)
    , m_frontend(0)
    , m_resourcesData(adoptPtr(new NetworkResourcesData()))
    , m_isRecalculatingStyle(false)
{
}

} // namespace WebCore
