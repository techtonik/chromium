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

#include "config.h"
#include "public/web/WebOptionElement.h"

#include "core/HTMLNames.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "public/platform/WebString.h"
#include "wtf/PassRefPtr.h"

namespace blink {

void WebOptionElement::setValue(const WebString& newValue)
{
    unwrap<HTMLOptionElement>()->setValue(newValue);
}

WebString WebOptionElement::value() const
{
    return constUnwrap<HTMLOptionElement>()->value();
}

int WebOptionElement::index() const
{
    return constUnwrap<HTMLOptionElement>()->index();
}

WebString WebOptionElement::text() const
{
    return constUnwrap<HTMLOptionElement>()->text();
}

WebString WebOptionElement::label() const
{
    return constUnwrap<HTMLOptionElement>()->label();
}

bool WebOptionElement::isEnabled() const
{
    return !(constUnwrap<HTMLOptionElement>()->isDisabledFormControl());
}

WebOptionElement::WebOptionElement(const PassRefPtrWillBeRawPtr<HTMLOptionElement>& elem)
    : WebElement(elem)
{
}

DEFINE_WEB_NODE_TYPE_CASTS(WebOptionElement, isHTMLOptionElement(constUnwrap<Node>()));

WebOptionElement& WebOptionElement::operator=(const PassRefPtrWillBeRawPtr<HTMLOptionElement>& elem)
{
    m_private = elem;
    return *this;
}

WebOptionElement::operator PassRefPtrWillBeRawPtr<HTMLOptionElement>() const
{
    return toHTMLOptionElement(m_private.get());
}

} // namespace blink
