// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/aura_init.h"

#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "components/mus/public/cpp/view.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace mandoline {

namespace {

std::set<std::string> GetResourcePaths(const std::string& resource_file) {
  std::set<std::string> paths;
  paths.insert(resource_file);
  return paths;
}

}  // namespace

// TODO(sky): the 1.f should be view->viewport_metrics().device_scale_factor,
// but that causes clipping problems. No doubt we're not scaling a size
// correctly.
AuraInit::AuraInit(mus::View* view,
                   mojo::Shell* shell,
                   const std::string& resource_file)
    : ui_init_(view->viewport_metrics().size_in_pixels.To<gfx::Size>(), 1.f),
      resource_file_(resource_file) {
  aura::Env::CreateInstance(false);

  InitializeResources(shell);

  ui::InitializeInputMethodForTesting();
}

AuraInit::~AuraInit() {
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (font_loader_.get()) {
    SkFontConfigInterface::SetGlobal(nullptr);
    // FontLoader is ref counted. We need to explicitly shutdown the background
    // thread, otherwise the background thread may be shutdown after the app is
    // torn down, when we're in a bad state.
    font_loader_->Shutdown();
  }
#endif
}

void AuraInit::InitializeResources(mojo::Shell* shell) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;
  resource_provider::ResourceLoader resource_loader(
      shell, GetResourcePaths(resource_file_));
  if (!resource_loader.BlockUntilLoaded())
    return;
  CHECK(resource_loader.loaded());
  base::i18n::InitializeICUWithFileDescriptor(
      resource_loader.GetICUFile().TakePlatformFile(),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      resource_loader.ReleaseFile(resource_file_), ui::SCALE_FACTOR_100P);

  // Initialize the skia font code to go ask fontconfig underneath.
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  font_loader_ = skia::AdoptRef(new font_service::FontLoader(shell));
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();
}

}  // namespace mandoline
