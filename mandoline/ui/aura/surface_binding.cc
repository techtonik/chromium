// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/surface_binding.h"

#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "mandoline/ui/aura/window_tree_host_mojo.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/cc/output_surface_mojo.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mandoline {

// PerConnectionState ----------------------------------------------------------

// State needed per ViewManager. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerConnectionState appropriate for the ViewManager. PerConnectionState is
// stored in a thread local map. When no more refereces to a PerConnectionState
// remain the PerConnectionState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerConnectionState
    : public base::RefCounted<PerConnectionState> {
 public:
  static PerConnectionState* Get(mojo::Shell* shell,
                                 mojo::ViewTreeConnection* connection);

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(mojo::View* view);

 private:
  typedef std::map<mojo::ViewTreeConnection*,
                   PerConnectionState*> ConnectionToStateMap;

  friend class base::RefCounted<PerConnectionState>;

  PerConnectionState(mojo::Shell* shell, mojo::ViewTreeConnection* connection);
  ~PerConnectionState();

  void Init();

  static base::LazyInstance<
      base::ThreadLocalPointer<ConnectionToStateMap>>::Leaky view_states;

  mojo::Shell* shell_;
  mojo::ViewTreeConnection* connection_;

  // Set of state needed to create an OutputSurface.
  mojo::GpuPtr gpu_;

  DISALLOW_COPY_AND_ASSIGN(PerConnectionState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerConnectionState::ConnectionToStateMap>>::Leaky
    SurfaceBinding::PerConnectionState::view_states;

// static
SurfaceBinding::PerConnectionState* SurfaceBinding::PerConnectionState::Get(
    mojo::Shell* shell,
    mojo::ViewTreeConnection* connection) {
  ConnectionToStateMap* view_map = view_states.Pointer()->Get();
  if (!view_map) {
    view_map = new ConnectionToStateMap;
    view_states.Pointer()->Set(view_map);
  }
  if (!(*view_map)[connection]) {
    (*view_map)[connection] = new PerConnectionState(shell, connection);
    (*view_map)[connection]->Init();
  }
  return (*view_map)[connection];
}

scoped_ptr<cc::OutputSurface>
SurfaceBinding::PerConnectionState::CreateOutputSurface(mojo::View* view) {
  // TODO(sky): figure out lifetime here. Do I need to worry about the return
  // value outliving this?
  mojo::CommandBufferPtr cb;
  gpu_->CreateOffscreenGLES2Context(GetProxy(&cb));

  scoped_refptr<cc::ContextProvider> context_provider(
      new mojo::ContextProviderMojo(cb.PassInterface().PassHandle()));
  return make_scoped_ptr(
      new mojo::OutputSurfaceMojo(context_provider, view->RequestSurface()));
}

SurfaceBinding::PerConnectionState::PerConnectionState(
    mojo::Shell* shell,
    mojo::ViewTreeConnection* connection)
    : shell_(shell) {
}

SurfaceBinding::PerConnectionState::~PerConnectionState() {
  ConnectionToStateMap* view_map = view_states.Pointer()->Get();
  DCHECK(view_map);
  DCHECK_EQ(this, (*view_map)[connection_]);
  view_map->erase(connection_);
  if (view_map->empty()) {
    delete view_map;
    view_states.Pointer()->Set(nullptr);
  }
}

void SurfaceBinding::PerConnectionState::Init() {
  mojo::ServiceProviderPtr service_provider;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:view_manager");
  shell_->ConnectToApplication(request.Pass(),
                               GetProxy(&service_provider),
                               nullptr,
                               nullptr);
  ConnectToService(service_provider.get(), &gpu_);
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(mojo::Shell* shell, mojo::View* view)
    : view_(view),
      state_(PerConnectionState::Get(shell, view->connection())) {
}

SurfaceBinding::~SurfaceBinding() {
}

scoped_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_->CreateOutputSurface(view_);
}

}  // namespace mandoline
