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
#include "public/web/WebSecurityPolicy.h"

#include "core/loader/FrameLoader.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

void WebSecurityPolicy::registerURLSchemeAsLocal(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsLocal(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsNoAccess(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsNoAccess(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsDisplayIsolated(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsRestrictingMixedContent(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsRestrictingMixedContent(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsSecure(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsSecure(scheme);
}

bool WebSecurityPolicy::shouldTreatURLSchemeAsSecure(const WebString& scheme)
{
    return SchemeRegistry::shouldTreatURLSchemeAsSecure(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsCORSEnabled(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsCORSEnabled(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsAllowingServiceWorkers(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsAllowingServiceWorkers(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsSupportingFetchAPI(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsSupportingFetchAPI(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(const WebString& scheme, PolicyAreas policyAreas)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme, static_cast<SchemeRegistry::PolicyAreas>(policyAreas));
}

void WebSecurityPolicy::registerURLSchemeAsFirstPartyWhenTopLevel(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsFirstPartyWhenTopLevel(scheme);
}

void WebSecurityPolicy::registerURLSchemeAsEmptyDocument(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsEmptyDocument(scheme);
}

void WebSecurityPolicy::addOriginAccessWhitelistEntry(
    const WebURL& sourceOrigin,
    const WebString& destinationProtocol,
    const WebString& destinationHost,
    bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessWhitelistEntry(
        *SecurityOrigin::create(sourceOrigin), destinationProtocol,
        destinationHost, allowDestinationSubdomains);
}

void WebSecurityPolicy::removeOriginAccessWhitelistEntry(
    const WebURL& sourceOrigin,
    const WebString& destinationProtocol,
    const WebString& destinationHost,
    bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessWhitelistEntry(
        *SecurityOrigin::create(sourceOrigin), destinationProtocol,
        destinationHost, allowDestinationSubdomains);
}

void WebSecurityPolicy::resetOriginAccessWhitelists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
}

void WebSecurityPolicy::addOriginTrustworthyWhiteList(const WebSecurityOrigin& origin)
{
    SecurityPolicy::addOriginTrustworthyWhiteList(origin);
}

WebString WebSecurityPolicy::generateReferrerHeader(WebReferrerPolicy referrerPolicy, const WebURL& url, const WebString& referrer)
{
    return SecurityPolicy::generateReferrer(static_cast<ReferrerPolicy>(referrerPolicy), url, referrer).referrer;
}

void WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(const WebString& scheme)
{
    SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(scheme);
}

} // namespace blink
