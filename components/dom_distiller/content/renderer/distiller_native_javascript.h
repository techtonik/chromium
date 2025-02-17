// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_NATIVE_JAVASCRIPT_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_NATIVE_JAVASCRIPT_H_

#include "components/dom_distiller/content/common/distiller_javascript_service.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "v8/include/v8.h"

namespace dom_distiller {

// This class is the primary interface for custom JavaScript introduced by the
// dom-distiller. Callbacks for JavaScript functionality and the communication
// channel to the browser process are maintained here.
class DistillerNativeJavaScript {
 public:
  explicit DistillerNativeJavaScript(content::RenderFrame* render_frame);
  ~DistillerNativeJavaScript();

  // Add the "distiller" JavaScript object and its functions to the current
  // |RenderFrame|.
  void AddJavaScriptObjectToFrame(v8::Local<v8::Context> context);

 private:
  // Add a function to the provided object.
  template<typename Sig>
  void BindFunctionToObject(v8::Local<v8::Object> javascript_object,
                            const std::string& name,
                            const base::Callback<Sig> callback);
  // Make sure the mojo service is connected.
  void EnsureServiceConnected();

  // Native code for "distiller.echo" in JavaScript. This simply returns the
  // provided string.
  std::string DistillerEcho(const std::string& message);

  // Send feedback about distillation quality.
  void DistillerSendFeedback(bool good);

  // Close the UI panel and record the action.
  void DistillerClosePanel();

  content::RenderFrame* render_frame_;
  DistillerJavaScriptServicePtr distiller_js_service_;
};

// static
v8::Local<v8::Object> GetOrCreateDistillerObject(v8::Isolate* isolate,
                                                 v8::Local<v8::Object> global);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_NATIVE_JAVASCRIPT_H_
