/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "core/fetch/ResourceFetcher.h"

#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoaderSet.h"
#include "core/fetch/UniqueIdentifier.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/ArchiveResourceCollection.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

#define PRELOAD_DEBUG 0

using blink::WebURLRequest;

namespace blink {

namespace {

// Events for UMA. Do not reorder or delete. Add new events at the end, but
// before SriResourceIntegrityMismatchEventCount.
enum SriResourceIntegrityMismatchEvent {
    CheckingForIntegrityMismatch = 0,
    RefetchDueToIntegrityMismatch = 1,
    SriResourceIntegrityMismatchEventCount
};

}

static void RecordSriResourceIntegrityMismatchEvent(SriResourceIntegrityMismatchEvent event)
{
    Platform::current()->histogramEnumeration("sri.resource_integrity_mismatch_event", event, SriResourceIntegrityMismatchEventCount);
}

static ResourceLoadPriority typeToPriority(Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        return ResourceLoadPriorityVeryHigh;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::CSSStyleSheet:
        return ResourceLoadPriorityHigh;
    case Resource::Raw:
    case Resource::Script:
    case Resource::Font:
    case Resource::ImportResource:
        return ResourceLoadPriorityMedium;
    case Resource::LinkSubresource:
    case Resource::TextTrack:
    case Resource::Media:
    case Resource::SVGDocument:
        return ResourceLoadPriorityLow;
    case Resource::Image:
    case Resource::LinkPrefetch:
    case Resource::LinkPreload:
        return ResourceLoadPriorityVeryLow;
    }

    ASSERT_NOT_REACHED();
    return ResourceLoadPriorityUnresolved;
}

ResourceLoadPriority ResourceFetcher::loadPriority(Resource::Type type, const FetchRequest& request, ResourcePriority::VisibilityStatus visibility)
{
    // TODO(yoav): Change it here so that priority can be changed even after it was resolved.
    if (request.priority() != ResourceLoadPriorityUnresolved)
        return request.priority();

    // Synchronous requests should always be max priority, lest they hang the renderer.
    if (request.options().synchronousPolicy == RequestSynchronously)
        return ResourceLoadPriorityHighest;

    return context().modifyPriorityForExperiments(typeToPriority(type), type, request, visibility);
}

static void populateResourceTiming(ResourceTimingInfo* info, Resource* resource, bool clearLoadTimings)
{
    info->setInitialRequest(resource->resourceRequest());
    info->setFinalResponse(resource->response());
    if (clearLoadTimings) {
        info->clearLoadTimings();
        info->setLoadFinishTime(info->initialTime());
    } else {
        info->setLoadFinishTime(resource->loadFinishTime());
    }
}

static WebURLRequest::RequestContext requestContextFromType(bool isMainFrame, Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        if (!isMainFrame)
            return WebURLRequest::RequestContextIframe;
        // FIXME: Change this to a context frame type (once we introduce them): http://fetch.spec.whatwg.org/#concept-request-context-frame-type
        return WebURLRequest::RequestContextHyperlink;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::CSSStyleSheet:
        return WebURLRequest::RequestContextStyle;
    case Resource::Script:
        return WebURLRequest::RequestContextScript;
    case Resource::Font:
        return WebURLRequest::RequestContextFont;
    case Resource::Image:
        return WebURLRequest::RequestContextImage;
    case Resource::Raw:
        return WebURLRequest::RequestContextSubresource;
    case Resource::ImportResource:
        return WebURLRequest::RequestContextImport;
    case Resource::LinkPrefetch:
        return WebURLRequest::RequestContextPrefetch;
    case Resource::LinkSubresource:
        return WebURLRequest::RequestContextSubresource;
    case Resource::LinkPreload:
        // TODO(yoav): We should give preload its own context:
        // https://github.com/whatwg/fetch/commit/26e5cca8ab5bb4b68a8f238f41dd7364d8c276b3
        return WebURLRequest::RequestContextSubresource;
    case Resource::TextTrack:
        return WebURLRequest::RequestContextTrack;
    case Resource::SVGDocument:
        return WebURLRequest::RequestContextImage;
    case Resource::Media: // TODO: Split this.
        return WebURLRequest::RequestContextVideo;
    }
    ASSERT_NOT_REACHED();
    return WebURLRequest::RequestContextSubresource;
}

ResourceFetcher::ResourceFetcher(FetchContext* context)
    : m_context(context)
    , m_garbageCollectDocumentResourcesTimer(this, &ResourceFetcher::garbageCollectDocumentResourcesTimerFired)
    , m_resourceTimingReportTimer(this, &ResourceFetcher::resourceTimingReportTimerFired)
    , m_autoLoadImages(true)
    , m_imagesEnabled(true)
    , m_allowStaleResources(false)
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
}

ResourceFetcher::~ResourceFetcher()
{
#if !ENABLE(OILPAN)
    clearPreloads();
#endif
}

WebTaskRunner* ResourceFetcher::loadingTaskRunner()
{
    if (!m_context)
        return nullptr;

    return m_context->loadingTaskRunner();
}

Resource* ResourceFetcher::cachedResource(const KURL& resourceURL) const
{
    KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
    return m_documentResources.get(url).get();
}

bool ResourceFetcher::canAccessResource(Resource* resource, SecurityOrigin* sourceOrigin, const KURL& url, AccessControlLoggingDecision logErrorsDecision) const
{
    // Redirects can change the response URL different from one of request.
    bool forPreload = resource->isUnusedPreload();
    if (!context().canRequest(resource->type(), resource->resourceRequest(), url, resource->options(), forPreload, FetchRequest::UseDefaultOriginRestrictionForType))
        return false;

    if (!sourceOrigin)
        sourceOrigin = context().securityOrigin();

    if (sourceOrigin->canRequestNoSuborigin(url))
        return true;

    String errorDescription;
    if (!resource->passesAccessControlCheck(sourceOrigin, errorDescription)) {
        resource->setCORSFailed();
        if (!forPreload && (logErrorsDecision == ShouldLogAccessControlErrors)) {
            String resourceType = Resource::resourceTypeToString(resource->type(), resource->options().initiatorInfo);
            context().addConsoleMessage(resourceType + " from origin '" + SecurityOrigin::create(url)->toString() + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + errorDescription);
        }
        return false;
    }
    return true;
}

bool ResourceFetcher::isControlledByServiceWorker() const
{
    return context().isControlledByServiceWorker();
}

bool ResourceFetcher::resourceNeedsLoad(Resource* resource, const FetchRequest& request, RevalidationPolicy policy)
{
    if (FetchRequest::DeferredByClient == request.defer())
        return false;
    if (policy != Use)
        return true;
    if (resource->stillNeedsLoad())
        return true;
    return request.options().synchronousPolicy == RequestSynchronously && resource->isLoading();
}

// Limit the number of URLs in m_validatedURLs to avoid memory bloat.
// http://crbug.com/52411
static const int kMaxValidatedURLsSize = 10000;

void ResourceFetcher::requestLoadStarted(Resource* resource, const FetchRequest& request, ResourceLoadStartType type, bool isStaticData)
{
    if (type == ResourceLoadingFromCache && resource->status() == Resource::Cached && !m_validatedURLs.contains(resource->url()))
        context().dispatchDidLoadResourceFromMemoryCache(resource);

    if (isStaticData)
        return;

    if (type == ResourceLoadingFromCache && !resource->stillNeedsLoad() && !m_validatedURLs.contains(request.resourceRequest().url())) {
        // Resources loaded from memory cache should be reported the first time they're used.
        OwnPtr<ResourceTimingInfo> info = ResourceTimingInfo::create(request.options().initiatorInfo.name, monotonicallyIncreasingTime(), resource->type() == Resource::MainResource);
        populateResourceTiming(info.get(), resource, true);
        m_scheduledResourceTimingReports.append(info.release());
        if (!m_resourceTimingReportTimer.isActive())
            m_resourceTimingReportTimer.startOneShot(0, FROM_HERE);
    }

    if (m_validatedURLs.size() >= kMaxValidatedURLsSize) {
        m_validatedURLs.clear();
    }
    m_validatedURLs.add(request.resourceRequest().url());
}

static PassRefPtr<TraceEvent::ConvertableToTraceFormat> urlForTraceEvent(const KURL& url)
{
    RefPtr<TracedValue> value = TracedValue::create();
    value->setString("url", url.string());
    return value.release();
}

void ResourceFetcher::preCacheData(const FetchRequest& request, const ResourceFactory& factory, const SubstituteData& substituteData)
{
    const KURL& url = request.resourceRequest().url();
    ASSERT(url.protocolIsData() || substituteData.isValid());
    if ((factory.type() == Resource::MainResource && !substituteData.isValid()) || factory.type() == Resource::Raw || factory.type() == Resource::Media)
        return;

    const String cacheIdentifier = getCacheIdentifier();
    if (Resource* oldResource = memoryCache()->resourceForURL(url, cacheIdentifier)) {
        if (!substituteData.isValid())
            return;
        memoryCache()->remove(oldResource);
    }

    WebString mimetype;
    WebString charset;
    RefPtr<SharedBuffer> data;
    if (substituteData.isValid()) {
        mimetype = substituteData.mimeType();
        charset = substituteData.textEncoding();
        data = substituteData.content();
    } else {
        data = PassRefPtr<SharedBuffer>(Platform::current()->parseDataURL(url, mimetype, charset));
        if (!data)
            return;
    }
    ResourceResponse response(url, mimetype, data->size(), charset, String());
    response.setHTTPStatusCode(200);
    response.setHTTPStatusText("OK");

    ResourcePtr<Resource> resource = factory.create(request.resourceRequest(), request.charset());
    resource->setNeedsSynchronousCacheHit(substituteData.forceSynchronousLoad());
    resource->setOptions(request.options());
    // FIXME: We should provide a body stream here.
    resource->responseReceived(response, nullptr);
    resource->setDataBufferingPolicy(BufferData);
    if (data->size())
        resource->setResourceBuffer(data);
    resource->setIdentifier(createUniqueIdentifier());
    resource->setCacheIdentifier(cacheIdentifier);
    resource->finish();
    memoryCache()->add(resource.get());
    scheduleDocumentResourcesGC();
}

ResourcePtr<Resource> ResourceFetcher::requestResource(FetchRequest& request, const ResourceFactory& factory, const SubstituteData& substituteData)
{
    ASSERT(request.options().synchronousPolicy == RequestAsynchronously || factory.type() == Resource::Raw || factory.type() == Resource::XSLStyleSheet);

    context().upgradeInsecureRequest(request);
    context().addClientHintsIfNecessary(request);
    context().addCSPHeaderIfNecessary(factory.type(), request);

    bool isStaticData = request.resourceRequest().url().protocolIsData() || substituteData.isValid();
    if (isStaticData)
        preCacheData(request, factory, substituteData);

    KURL url = request.resourceRequest().url();
    TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url", urlForTraceEvent(url));

    WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource '%s', charset '%s', priority=%d, forPreload=%u, type=%s", url.elidedString().latin1().data(), request.charset().latin1().data(), request.priority(), request.forPreload(), ResourceTypeName(factory.type()));

    // If only the fragment identifiers differ, it is the same resource.
    url = MemoryCache::removeFragmentIdentifierIfNeeded(url);

    if (!url.isValid())
        return nullptr;

    if (!context().canRequest(factory.type(), request.resourceRequest(), url, request.options(), request.forPreload(), request.originRestriction()))
        return nullptr;

    if (!request.forPreload()) {
        V8DOMActivityLogger* activityLogger = nullptr;
        if (request.options().initiatorInfo.name == FetchInitiatorTypeNames::xmlhttprequest)
            activityLogger = V8DOMActivityLogger::currentActivityLogger();
        else
            activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();

        if (activityLogger) {
            Vector<String> argv;
            argv.append(Resource::resourceTypeToString(factory.type(), request.options().initiatorInfo));
            argv.append(url);
            activityLogger->logEvent("blinkRequestResource", argv.size(), argv.data());
        }
    }

    // See if we can use an existing resource from the cache.
    ResourcePtr<Resource> resource = memoryCache()->resourceForURL(url, getCacheIdentifier());

    const RevalidationPolicy policy = determineRevalidationPolicy(factory.type(), request, resource.get(), isStaticData);
    switch (policy) {
    case Reload:
        memoryCache()->remove(resource.get());
        // Fall through
    case Load:
        resource = createResourceForLoading(request, request.charset(), factory);
        break;
    case Revalidate:
        resource = createResourceForRevalidation(request, resource.get(), factory);
        break;
    case Use:
        memoryCache()->updateForAccess(resource.get());
        break;
    }

    if (!resource)
        return nullptr;
    if (resource->type() != factory.type()) {
        ASSERT(request.forPreload());
        return nullptr;
    }

    if (!resource->hasClients())
        m_deadStatsRecorder.update(policy);

    if (policy != Use)
        resource->setIdentifier(createUniqueIdentifier());

    if (!request.forPreload() || policy != Use) {
        ResourceLoadPriority priority = loadPriority(factory.type(), request, ResourcePriority::NotVisible);
        // When issuing another request for a resource that is already in-flight make
        // sure to not demote the priority of the in-flight request. If the new request
        // isn't at the same priority as the in-flight request, only allow promotions.
        // This can happen when a visible image's priority is increased and then another
        // reference to the image is parsed (which would be at a lower priority).
        if (priority > resource->resourceRequest().priority())
            resource->didChangePriority(priority, 0);
    }

    if (resourceNeedsLoad(resource.get(), request, policy)) {
        if (!context().shouldLoadNewResource(factory.type())) {
            if (memoryCache()->contains(resource.get()))
                memoryCache()->remove(resource.get());
            return nullptr;
        }

        if (!scheduleArchiveLoad(resource.get(), request.resourceRequest()))
            resource->load(this, request.options());

        // For asynchronous loads that immediately fail, it's sufficient to return a
        // null Resource, as it indicates that something prevented the load from starting.
        // If there's a network error, that failure will happen asynchronously. However, if
        // a sync load receives a network error, it will have already happened by this point.
        // In that case, the requester should have access to the relevant ResourceError, so
        // we need to return a non-null Resource.
        if (resource->errorOccurred()) {
            if (memoryCache()->contains(resource.get()))
                memoryCache()->remove(resource.get());
            return request.options().synchronousPolicy == RequestSynchronously ? resource : nullptr;
        }
    }

    // FIXME: Temporarily leave main resource caching disabled for chromium,
    // see https://bugs.webkit.org/show_bug.cgi?id=107962. Before caching main
    // resources, we should be sure to understand the implications for memory
    // use.
    // Remove main resource from cache to prevent reuse.
    if (factory.type() == Resource::MainResource) {
        ASSERT(policy != Use || substituteData.isValid());
        ASSERT(policy != Revalidate);
        memoryCache()->remove(resource.get());
    }

    requestLoadStarted(resource.get(), request, policy == Use ? ResourceLoadingFromCache : ResourceLoadingFromNetwork, isStaticData);

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url(), resource);
    return resource;
}

void ResourceFetcher::resourceTimingReportTimerFired(Timer<ResourceFetcher>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_resourceTimingReportTimer);
    Vector<OwnPtr<ResourceTimingInfo>> timingReports;
    timingReports.swap(m_scheduledResourceTimingReports);
    for (const auto& timingInfo : timingReports)
        context().addResourceTiming(*timingInfo);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request, Resource::Type type)
{
    WebURLRequest::RequestContext requestContext = requestContextFromType(context().isMainFrame(), type);
    request.setRequestContext(requestContext);
}

void ResourceFetcher::initializeResourceRequest(ResourceRequest& request, Resource::Type type)
{
    if (request.cachePolicy() == UseProtocolCachePolicy)
        request.setCachePolicy(context().resourceRequestCachePolicy(request, type));
    if (request.requestContext() == WebURLRequest::RequestContextUnspecified)
        determineRequestContext(request, type);
    if (type == Resource::LinkPrefetch || type == Resource::LinkSubresource)
        request.setHTTPHeaderField("Purpose", "prefetch");

    context().addAdditionalRequestHeaders(request, (type == Resource::MainResource) ? FetchMainResource : FetchSubresource);
}

ResourcePtr<Resource> ResourceFetcher::createResourceForRevalidation(const FetchRequest& request, Resource* resource, const ResourceFactory& factory)
{
    ASSERT(resource);
    ASSERT(memoryCache()->contains(resource));
    ASSERT(resource->isLoaded());
    ASSERT(resource->canUseCacheValidator());
    ASSERT(!resource->resourceToRevalidate());
    ASSERT(!context().isControlledByServiceWorker());

    ResourceRequest revalidatingRequest(resource->resourceRequest());
    revalidatingRequest.clearHTTPReferrer();
    initializeResourceRequest(revalidatingRequest, resource->type());

    const AtomicString& lastModified = resource->response().httpHeaderField("Last-Modified");
    const AtomicString& eTag = resource->response().httpHeaderField("ETag");
    if (!lastModified.isEmpty() || !eTag.isEmpty()) {
        ASSERT(context().cachePolicy() != CachePolicyReload);
        if (context().cachePolicy() == CachePolicyRevalidate)
            revalidatingRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
    }
    if (!lastModified.isEmpty())
        revalidatingRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
    if (!eTag.isEmpty())
        revalidatingRequest.setHTTPHeaderField("If-None-Match", eTag);

    double stalenessLifetime = resource->stalenessLifetime();
    if (std::isfinite(stalenessLifetime) && stalenessLifetime > 0) {
        revalidatingRequest.setHTTPHeaderField("Resource-Freshness", AtomicString(String::format("max-age=%.0lf,stale-while-revalidate=%.0lf,age=%.0lf", resource->freshnessLifetime(), stalenessLifetime, resource->currentAge())));
    }

    ResourcePtr<Resource> newResource = factory.create(revalidatingRequest, resource->encoding());
    WTF_LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), resource);

    newResource->setResourceToRevalidate(resource);
    newResource->setCacheIdentifier(resource->cacheIdentifier());

    memoryCache()->remove(resource);
    memoryCache()->add(newResource.get());
    return newResource;
}

ResourcePtr<Resource> ResourceFetcher::createResourceForLoading(FetchRequest& request, const String& charset, const ResourceFactory& factory)
{
    const String cacheIdentifier = getCacheIdentifier();
    ASSERT(!memoryCache()->resourceForURL(request.resourceRequest().url(), cacheIdentifier));

    WTF_LOG(ResourceLoading, "Loading Resource for '%s'.", request.resourceRequest().url().elidedString().latin1().data());

    initializeResourceRequest(request.mutableResourceRequest(), factory.type());
    ResourcePtr<Resource> resource = factory.create(request.resourceRequest(), charset);
    resource->setCacheIdentifier(cacheIdentifier);

    memoryCache()->add(resource.get());
    return resource;
}

void ResourceFetcher::storeResourceTimingInitiatorInformation(Resource* resource)
{
    if (resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal)
        return;

    OwnPtr<ResourceTimingInfo> info = ResourceTimingInfo::create(resource->options().initiatorInfo.name, monotonicallyIncreasingTime(), resource->type() == Resource::MainResource);

    if (resource->isCacheValidator()) {
        const AtomicString& timingAllowOrigin = resource->resourceToRevalidate()->response().httpHeaderField("Timing-Allow-Origin");
        if (!timingAllowOrigin.isEmpty())
            info->setOriginalTimingAllowOrigin(timingAllowOrigin);
    }

    if (resource->type() != Resource::MainResource || context().updateTimingInfoForIFrameNavigation(info.get()))
        m_resourceTimingInfoMap.add(resource, info.release());
}

ResourceFetcher::RevalidationPolicy ResourceFetcher::determineRevalidationPolicy(Resource::Type type, const FetchRequest& fetchRequest, Resource* existingResource, bool isStaticData) const
{
    const ResourceRequest& request = fetchRequest.resourceRequest();

    if (!existingResource)
        return Load;

    // Checks if the resource has an explicit policy about integrity metadata.
    // Currently only applies to ScriptResources.
    //
    // This is necessary because ScriptResource objects do not keep the raw
    // data around after the source is accessed once, so if the resource is
    // accessed from the MemoryCache for a second time, there is no way to redo
    // an integrity check.
    //
    // Thus, Blink implements a scheme where it caches the integrity
    // information for a ScriptResource after the first time it is checked, and
    // if there is another request for that resource, with the same integrity
    // metadata, Blink skips the integrity calculation. However, if the
    // integrity metadata is a mismatch, the MemoryCache must be skipped here,
    // and a new request for the resource must be made to get the raw data.
    // This is expected to be an uncommon case, however, as it implies two
    // same-origin requests to the same resource, but with different integrity
    // metadata.
    RecordSriResourceIntegrityMismatchEvent(CheckingForIntegrityMismatch);
    if (existingResource->mustRefetchDueToIntegrityMetadata(fetchRequest)) {
        RecordSriResourceIntegrityMismatchEvent(RefetchDueToIntegrityMismatch);
        return Reload;
    }

    // Service Worker's CORS fallback message must not be cached.
    if (existingResource->response().wasFallbackRequiredByServiceWorker())
        return Reload;

    // We already have a preload going for this URL.
    if (fetchRequest.forPreload() && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
        // We really should discard the new prefetch since the preload has more
        // specific type information! crbug.com/379893
        // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to type mismatch.");
        return Reload;
    }

    // Do not load from cache if images are not enabled. The load for this image will be blocked
    // in ImageResource::load.
    if (FetchRequest::DeferredByClient == fetchRequest.defer())
        return Reload;

    // Never use cache entries for downloadToFile / useStreamOnResponse
    // requests. The data will be delivered through other paths.
    if (request.downloadToFile() || request.useStreamOnResponse())
        return Reload;

    // If resource was populated from a SubstituteData load or data: url, use it.
    if (isStaticData)
        return Use;

    if (!existingResource->canReuse(request))
        return Reload;

    // Certain requests (e.g., XHRs) might have manually set headers that require revalidation.
    // FIXME: In theory, this should be a Revalidate case. In practice, the MemoryCache revalidation path assumes a whole bunch
    // of things about how revalidation works that manual headers violate, so punt to Reload instead.
    if (request.isConditional())
        return Reload;

    // Don't reload resources while pasting.
    if (m_allowStaleResources)
        return Use;

    if (request.cachePolicy() == ResourceRequestCachePolicy::ReloadBypassingCache)
        return Reload;

    if (!fetchRequest.options().canReuseRequest(existingResource->options()))
        return Reload;

    // Always use preloads.
    if (existingResource->isPreloaded())
        return Use;

    // CachePolicyHistoryBuffer uses the cache no matter what.
    CachePolicy cachePolicy = context().cachePolicy();
    if (cachePolicy == CachePolicyHistoryBuffer)
        return Use;

    // Don't reuse resources with Cache-control: no-store.
    if (existingResource->hasCacheControlNoStoreHeader()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to Cache-control: no-store.");
        return Reload;
    }

    // If credentials were sent with the previous request and won't be
    // with this one, or vice versa, re-fetch the resource.
    //
    // This helps with the case where the server sends back
    // "Access-Control-Allow-Origin: *" all the time, but some of the
    // client's requests are made without CORS and some with.
    if (existingResource->resourceRequest().allowStoredCredentials() != request.allowStoredCredentials()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to difference in credentials settings.");
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document,
    // even if the cache policies would tell us to.
    // We also group loads of the same resource together.
    // Raw resources are exempted, as XHRs fall into this category and may have user-set Cache-Control:
    // headers or other factors that require separate requests.
    if (type != Resource::Raw) {
        if (!context().isLoadComplete() && m_validatedURLs.contains(existingResource->url()))
            return Use;
        // TODO(japhet): existingResource->isLoading() and existingResource->loader() are not identical,
        // which is lame.
        // Being in the loading state and having a ResourceLoader* are subtly diffent cases, either of which
        // should indicate reuse. A resource can have isLoading() return true without a ResourceLoader* if
        // it is a font that defers actually loading until the font is required. On the other hand,
        // a Resource can have a non-null ResourceLoader* but have isLoading() return false in a narrow window
        // during completion, because we set loading to false before notifying ResourceClients, but don't
        // clear the ResourceLoader pointer until the stack unwinds. If, inside the ResourceClient callbacks,
        // an event fires synchronously and an event handler re-requests the resource, we can reach this point
        // while not loading but having a ResourceLoader.
        if (existingResource->isLoading() || existingResource->loader())
            return Use;
    }

    // CachePolicyReload always reloads
    if (cachePolicy == CachePolicyReload) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to CachePolicyReload.");
        return Reload;
    }

    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicye reloading due to resource being in the error state");
        return Reload;
    }

    // List of available images logic allows images to be re-used without cache validation. We restrict this only to images
    // from memory cache which are the same as the version in the current document.
    if (type == Resource::Image && existingResource == cachedResource(request.url()))
        return Use;

    // Defer to the browser process cache for Vary header handling.
    if (existingResource->hasVaryHeader())
        return Reload;

    // If any of the redirects in the chain to loading the resource were not cacheable, we cannot reuse our cached resource.
    if (!existingResource->canReuseRedirectChain()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to an uncacheable redirect");
        return Reload;
    }

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (cachePolicy == CachePolicyRevalidate || existingResource->mustRevalidateDueToCacheHeaders()
        || request.cacheControlContainsNoCache()) {
        // See if the resource has usable ETag or Last-modified headers.
        // If the page is controlled by the ServiceWorker, we choose the Reload policy because the revalidation headers should not be exposed to the ServiceWorker.(crbug.com/429570)
        if (existingResource->canUseCacheValidator() && !context().isControlledByServiceWorker())
            return Revalidate;

        // No, must reload.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to missing cache validators.");
        return Reload;
    }

    return Use;
}

void ResourceFetcher::setAutoLoadImages(bool enable)
{
    if (enable == m_autoLoadImages)
        return;

    m_autoLoadImages = enable;

    if (!m_autoLoadImages)
        return;

    reloadImagesIfNotDeferred();
}

void ResourceFetcher::setImagesEnabled(bool enable)
{
    if (enable == m_imagesEnabled)
        return;

    m_imagesEnabled = enable;

    if (!m_imagesEnabled)
        return;

    reloadImagesIfNotDeferred();
}

bool ResourceFetcher::clientDefersImage(const KURL& url) const
{
    return !context().allowImage(m_imagesEnabled, url);
}

bool ResourceFetcher::shouldDeferImageLoad(const KURL& url) const
{
    return clientDefersImage(url) || !m_autoLoadImages;
}

void ResourceFetcher::reloadImagesIfNotDeferred()
{
    for (const auto& documentResource : m_documentResources) {
        Resource* resource = documentResource.value.get();
        if (resource->type() == Resource::Image && resource->stillNeedsLoad() && !clientDefersImage(resource->url()))
            const_cast<Resource*>(resource)->load(this, defaultResourceOptions());
    }
}

void ResourceFetcher::redirectReceived(Resource* resource, const ResourceResponse& redirectResponse)
{
    ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
    if (it != m_resourceTimingInfoMap.end())
        it->value->addRedirect(redirectResponse);
}

void ResourceFetcher::didLoadResource()
{
    scheduleDocumentResourcesGC();
    context().didLoadResource();
}

void ResourceFetcher::scheduleDocumentResourcesGC()
{
    if (!m_garbageCollectDocumentResourcesTimer.isActive())
        m_garbageCollectDocumentResourcesTimer.startOneShot(0, FROM_HERE);
}

// Garbage collecting m_documentResources is a workaround for the
// ResourcePtrs on the RHS being strong references. Ideally this
// would be a weak map, however ResourcePtrs perform additional
// bookkeeping on Resources, so instead pseudo-GC them -- when the
// reference count reaches 1, m_documentResources is the only reference, so
// remove it from the map.
void ResourceFetcher::garbageCollectDocumentResourcesTimerFired(Timer<ResourceFetcher>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_garbageCollectDocumentResourcesTimer);
    garbageCollectDocumentResources();
}

void ResourceFetcher::garbageCollectDocumentResources()
{
    typedef Vector<String, 10> StringVector;
    StringVector resourcesToDelete;

    for (const auto& documentResource : m_documentResources) {
        if (documentResource.value->hasOneHandle())
            resourcesToDelete.append(documentResource.key);
    }

    m_documentResources.removeAll(resourcesToDelete);
}

int ResourceFetcher::requestCount() const
{
    return m_loaders ? m_loaders->size() : 0;
}

void ResourceFetcher::preloadStarted(Resource* resource)
{
    if (m_preloads && m_preloads->contains(resource))
        return;
    TRACE_EVENT_ASYNC_STEP_INTO0("blink.net", "Resource", resource, "Preload");
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = adoptPtrWillBeNoop(new WillBeHeapListHashSet<RawPtrWillBeMember<Resource>>);
    m_preloads->add(resource);

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().string().latin1().data());
#endif
}

bool ResourceFetcher::isPreloaded(const KURL& url) const
{
    if (m_preloads) {
        for (auto resource : *m_preloads) {
            if (resource->url() == url)
                return true;
        }
    }

    return false;
}

void ResourceFetcher::clearPreloads()
{
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    if (!m_preloads)
        return;

    for (auto resource : *m_preloads) {
        resource->decreasePreloadCount();
        bool deleted = resource->deleteIfPossible();
        if (!deleted && resource->preloadResult() == Resource::PreloadNotReferenced)
            memoryCache()->remove(resource.get());
    }
    m_preloads.clear();
}

void ResourceFetcher::addAllArchiveResources(MHTMLArchive* archive)
{
    ASSERT(archive);
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = ArchiveResourceCollection::create();
    m_archiveResourceCollection->addAllResources(archive);
}

bool ResourceFetcher::scheduleArchiveLoad(Resource* resource, const ResourceRequest& request)
{
    if (!m_archiveResourceCollection)
        return false;

    ArchiveResource* archiveResource = m_archiveResourceCollection->archiveResourceForURL(request.url());
    if (!archiveResource) {
        resource->error(Resource::LoadError);
        return false;
    }

    resource->setLoading(true);
    resource->responseReceived(archiveResource->response(), nullptr);
    SharedBuffer* data = archiveResource->data();
    if (data)
        resource->appendData(data->data(), data->size());
    resource->finish();
    return true;
}

void ResourceFetcher::didFinishLoading(Resource* resource, double finishTime, int64_t encodedDataLength)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource);
    willTerminateResourceLoader(resource->loader());

    if (resource && resource->response().isHTTP() && resource->response().httpStatusCode() < 400) {
        ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
        if (it != m_resourceTimingInfoMap.end()) {
            OwnPtr<ResourceTimingInfo> info = it->value.release();
            m_resourceTimingInfoMap.remove(it);
            populateResourceTiming(info.get(), resource, false);
            if (resource->options().requestInitiatorContext == DocumentContext)
                context().addResourceTiming(*info);
            resource->reportResourceTimingToClients(*info);
        }
    }
    context().dispatchDidFinishLoading(resource->identifier(), finishTime, encodedDataLength);
}

void ResourceFetcher::didFailLoading(const Resource* resource, const ResourceError& error)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource);
    willTerminateResourceLoader(resource->loader());
    bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
    context().dispatchDidFail(resource->identifier(), error, isInternalRequest);
}

void ResourceFetcher::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    context().dispatchWillSendRequest(identifier, request, redirectResponse, initiatorInfo);
}

void ResourceFetcher::didReceiveResponse(const Resource* resource, const ResourceResponse& response)
{
    // If the response is fetched via ServiceWorker, the original URL of the response could be different from the URL of the request.
    // We check the URL not to load the resources which are forbidden by the page CSP. This behavior is not specified in the CSP specification yet.
    // FIXME(mkwst): Fix this behavior when the CSP docs are updated.
    if (response.wasFetchedViaServiceWorker()) {
        const KURL& originalURL = response.originalURLViaServiceWorker();
        if (!originalURL.isEmpty() && !context().canRequest(resource->type(), resource->resourceRequest(), originalURL, resource->options(), false, FetchRequest::UseDefaultOriginRestrictionForType)) {
            resource->loader()->cancel();
            bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
            context().dispatchDidFail(resource->identifier(), ResourceError(errorDomainBlinkInternal, 0, originalURL.string(), "Unsafe attempt to load URL " + originalURL.elidedString() + " fetched by a ServiceWorker."), isInternalRequest);
            return;
        }
    }
    context().dispatchDidReceiveResponse(resource->identifier(), response, resource->loader());
}

void ResourceFetcher::didReceiveData(const Resource* resource, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(resource->identifier(), data, dataLength, encodedDataLength);
}

void ResourceFetcher::didDownloadData(const Resource* resource, int dataLength, int encodedDataLength)
{
    context().dispatchDidDownloadData(resource->identifier(), dataLength, encodedDataLength);
}

void ResourceFetcher::acceptDataFromThreadedReceiver(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(identifier, data, dataLength, encodedDataLength);
}

void ResourceFetcher::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    if (!m_nonBlockingLoaders)
        m_nonBlockingLoaders = ResourceLoaderSet::create();
    m_nonBlockingLoaders->add(loader);
    m_loaders->remove(loader);
    context().didLoadResource();
}

void ResourceFetcher::didInitializeResourceLoader(ResourceLoader* loader)
{
    if (loader->cachedResource()->shouldBlockLoadEvent()) {
        if (!m_loaders)
            m_loaders = ResourceLoaderSet::create();
        m_loaders->add(loader);
    } else {
        if (!m_nonBlockingLoaders)
            m_nonBlockingLoaders = ResourceLoaderSet::create();
        m_nonBlockingLoaders->add(loader);
    }
}

void ResourceFetcher::willTerminateResourceLoader(ResourceLoader* loader)
{
    if (m_loaders && m_loaders->contains(loader))
        m_loaders->remove(loader);
    else if (m_nonBlockingLoaders && m_nonBlockingLoaders->contains(loader))
        m_nonBlockingLoaders->remove(loader);
    else
        ASSERT_NOT_REACHED();
}

void ResourceFetcher::willStartLoadingResource(Resource* resource, ResourceRequest& request)
{
    context().willStartLoadingResource(request);
    storeResourceTimingInitiatorInformation(resource);
    TRACE_EVENT_ASYNC_BEGIN2("blink.net", "Resource", resource, "url", resource->url().string().ascii(), "priority", resource->resourceRequest().priority());
}

void ResourceFetcher::stopFetching()
{
    if (m_nonBlockingLoaders)
        m_nonBlockingLoaders->cancelAll();
    if (m_loaders)
        m_loaders->cancelAll();
}

bool ResourceFetcher::isFetching() const
{
    return m_loaders && !m_loaders->isEmpty();
}

void ResourceFetcher::setDefersLoading(bool defers)
{
    if (m_loaders)
        m_loaders->setAllDefersLoading(defers);
    if (m_nonBlockingLoaders)
        m_nonBlockingLoaders->setAllDefersLoading(defers);
}

bool ResourceFetcher::defersLoading() const
{
    return context().defersLoading();
}

bool ResourceFetcher::isLoadedBy(ResourceFetcher* possibleOwner) const
{
    return this == possibleOwner;
}

bool ResourceFetcher::canAccessRedirect(Resource* resource, ResourceRequest& newRequest, const ResourceResponse& redirectResponse, ResourceLoaderOptions& options)
{
    if (!context().canRequest(resource->type(), newRequest, newRequest.url(), options, resource->isUnusedPreload(), FetchRequest::UseDefaultOriginRestrictionForType))
        return false;
    if (options.corsEnabled == IsCORSEnabled) {
        SecurityOrigin* sourceOrigin = options.securityOrigin.get();
        if (!sourceOrigin)
            sourceOrigin = context().securityOrigin();

        String errorMessage;
        StoredCredentials withCredentials = resource->lastResourceRequest().allowStoredCredentials() ? AllowStoredCredentials : DoNotAllowStoredCredentials;
        if (!CrossOriginAccessControl::handleRedirect(sourceOrigin, newRequest, redirectResponse, withCredentials, options, errorMessage)) {
            resource->setCORSFailed();
            context().addConsoleMessage(errorMessage);
            return false;
        }
    }
    if (resource->type() == Resource::Image && shouldDeferImageLoad(newRequest.url()))
        return false;
    return true;
}

void ResourceFetcher::updateAllImageResourcePriorities()
{
    if (!m_loaders)
        return;

    TRACE_EVENT0("blink", "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");
    for (const auto& loader : m_loaders->hashSet()) {
        Resource* resource = loader->cachedResource();
        if (!resource->isImage())
            continue;

        ResourcePriority resourcePriority = resource->priorityFromClients();
        ResourceLoadPriority resourceLoadPriority = loadPriority(Resource::Image, FetchRequest(resource->resourceRequest(), FetchInitiatorInfo()), resourcePriority.visibility);
        if (resourceLoadPriority == resource->resourceRequest().priority())
            continue;

        resource->didChangePriority(resourceLoadPriority, resourcePriority.intraPriorityValue);
        TRACE_EVENT_ASYNC_STEP_INTO1("blink.net", "Resource", resource, "ChangePriority", "priority", resourceLoadPriority);
        context().dispatchDidChangeResourcePriority(resource->identifier(), resourceLoadPriority, resourcePriority.intraPriorityValue);
    }
}

#if PRELOAD_DEBUG
void ResourceFetcher::printPreloadStats()
{
    if (!m_preloads)
        return;

    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    for (auto resource : *m_preloads) {
        if (resource->preloadResult() == Resource::PreloadNotReferenced)
            printf("!! UNREFERENCED PRELOAD %s\n", resource->url().string().latin1().data());
        else if (resource->preloadResult() == Resource::PreloadReferencedWhileComplete)
            printf("HIT COMPLETE PRELOAD %s\n", resource->url().string().latin1().data());
        else if (resource->preloadResult() == Resource::PreloadReferencedWhileLoading)
            printf("HIT LOADING PRELOAD %s\n", resource->url().string().latin1().data());

        if (resource->type() == Resource::Script) {
            scripts++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                scriptMisses++;
        } else if (resource->type() == Resource::CSSStyleSheet) {
            stylesheets++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                stylesheetMisses++;
        } else {
            images++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                imageMisses++;
        }

        if (resource->errorOccurred())
            memoryCache()->remove(resource.get());

        resource->decreasePreloadCount();
    }
    m_preloads.clear();

    if (scripts)
        printf("SCRIPTS: %d (%d hits, hit rate %d%%)\n", scripts, scripts - scriptMisses, (scripts - scriptMisses) * 100 / scripts);
    if (stylesheets)
        printf("STYLESHEETS: %d (%d hits, hit rate %d%%)\n", stylesheets, stylesheets - stylesheetMisses, (stylesheets - stylesheetMisses) * 100 / stylesheets);
    if (images)
        printf("IMAGES:  %d (%d hits, hit rate %d%%)\n", images, images - imageMisses, (images - imageMisses) * 100 / images);
}
#endif

const ResourceLoaderOptions& ResourceFetcher::defaultResourceOptions()
{
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, options, (BufferData, AllowStoredCredentials, ClientRequestedCredentials, CheckContentSecurityPolicy, DocumentContext));
    return options;
}

String ResourceFetcher::getCacheIdentifier() const
{
    if (context().isControlledByServiceWorker())
        return String::number(context().serviceWorkerID());
    return MemoryCache::defaultCacheIdentifier();
}

ResourceFetcher::DeadResourceStatsRecorder::DeadResourceStatsRecorder()
    : m_useCount(0)
    , m_revalidateCount(0)
    , m_loadCount(0)
{
}

ResourceFetcher::DeadResourceStatsRecorder::~DeadResourceStatsRecorder()
{
    Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.HitCount", m_useCount, 0, 1000, 50);
    Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.RevalidateCount", m_revalidateCount, 0, 1000, 50);
    Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.LoadCount", m_loadCount, 0, 1000, 50);
}

void ResourceFetcher::DeadResourceStatsRecorder::update(RevalidationPolicy policy)
{
    switch (policy) {
    case Reload:
    case Load:
        ++m_loadCount;
        return;
    case Revalidate:
        ++m_revalidateCount;
        return;
    case Use:
        ++m_useCount;
        return;
    }
}

DEFINE_TRACE(ResourceFetcher)
{
    visitor->trace(m_context);
    visitor->trace(m_archiveResourceCollection);
    visitor->trace(m_loaders);
    visitor->trace(m_nonBlockingLoaders);
#if ENABLE(OILPAN)
    visitor->trace(m_preloads);
    visitor->trace(m_resourceTimingInfoMap);
#endif
}

}
