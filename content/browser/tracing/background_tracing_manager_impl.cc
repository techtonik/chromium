// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include "base/cpu.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "gpu/config/gpu_info.h"
#include "net/base/network_change_notifier.h"

namespace content {

namespace {

base::LazyInstance<BackgroundTracingManagerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

// These values are used for a histogram. Do not reorder.
enum BackgroundTracingMetrics {
  SCENARIO_ACTIVATION_REQUESTED = 0,
  SCENARIO_ACTIVATED_SUCCESSFULLY = 1,
  RECORDING_ENABLED = 2,
  PREEMPTIVE_TRIGGERED = 3,
  REACTIVE_TRIGGERED = 4,
  FINALIZATION_ALLOWED = 5,
  FINALIZATION_DISALLOWED = 6,
  FINALIZATION_STARTED = 7,
  FINALIZATION_COMPLETE = 8,
  NUMBER_OF_BACKGROUND_TRACING_METRICS,
};

void RecordBackgroundTracingMetric(BackgroundTracingMetrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

std::string GetNetworkTypeString() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    default:
      break;
  }
  return "Unknown";
}

}  // namespace

BackgroundTracingManagerImpl::TracingTimer::TracingTimer(
    StartedFinalizingCallback callback) : callback_(callback) {
}

BackgroundTracingManagerImpl::TracingTimer::~TracingTimer() {
}

void BackgroundTracingManagerImpl::TracingTimer::StartTimer(int seconds) {
  tracing_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(seconds), this,
      &BackgroundTracingManagerImpl::TracingTimer::TracingTimerFired);
}

void BackgroundTracingManagerImpl::TracingTimer::CancelTimer() {
  tracing_timer_.Stop();
}

void BackgroundTracingManagerImpl::TracingTimer::TracingTimerFired() {
  BackgroundTracingManagerImpl::GetInstance()->BeginFinalizing(callback_);
}

void BackgroundTracingManagerImpl::TracingTimer::FireTimerForTesting() {
  CancelTimer();
  TracingTimerFired();
}

BackgroundTracingManager* BackgroundTracingManager::GetInstance() {
  return BackgroundTracingManagerImpl::GetInstance();
}

BackgroundTracingManagerImpl* BackgroundTracingManagerImpl::GetInstance() {
  return g_controller.Pointer();
}

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()),
      is_gathering_(false),
      is_tracing_(false),
      requires_anonymized_data_(true),
      trigger_handle_ids_(0),
      reactive_triggered_handle_(-1) {}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() {
  NOTREACHED();
}

void BackgroundTracingManagerImpl::WhenIdle(
    base::Callback<void()> idle_callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  idle_callback_ = idle_callback;
}

void BackgroundTracingManagerImpl::TriggerPreemptiveFinalization() {
  CHECK(config_ &&
        config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE);

  if (!is_tracing_ || is_gathering_)
    return;

  RecordBackgroundTracingMetric(PREEMPTIVE_TRIGGERED);
  BeginFinalizing(StartedFinalizingCallback());
}

void BackgroundTracingManagerImpl::OnHistogramTrigger(
    const std::string& histogram_name) {
  for (auto& rule : config_->rules()) {
    static_cast<BackgroundTracingRule*>(rule)
        ->OnHistogramTrigger(histogram_name);
  }
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    scoped_ptr<BackgroundTracingConfig> config,
    const BackgroundTracingManager::ReceiveCallback& receive_callback,
    DataFiltering data_filtering) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RecordBackgroundTracingMetric(SCENARIO_ACTIVATION_REQUESTED);

  if (is_tracing_)
    return false;

  bool requires_anonymized_data = (data_filtering == ANONYMIZE_DATA);

  // If the I/O thread isn't running, this is a startup scenario and
  // we have to wait until initialization is finished to validate that the
  // scenario can run.
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    // TODO(oysteine): Retry when time_until_allowed has elapsed.
    if (config && delegate_ &&
        !delegate_->IsAllowedToBeginBackgroundScenario(
            *config.get(), requires_anonymized_data)) {
      return false;
    }
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::ValidateStartupScenario,
                   base::Unretained(this)));
  }

  // No point in tracing if there's nowhere to send it.
  if (config && receive_callback.is_null())
    return false;

  config_.reset(static_cast<BackgroundTracingConfigImpl*>(config.release()));
  receive_callback_ = receive_callback;
  requires_anonymized_data_ = requires_anonymized_data;

  if (config_) {
    DCHECK(!config_.get()->rules().empty());
    for (auto& rule : config_.get()->rules())
      static_cast<BackgroundTracingRule*>(rule)->Install();
  }

  EnableRecordingIfConfigNeedsIt();

  RecordBackgroundTracingMetric(SCENARIO_ACTIVATED_SUCCESSFULLY);
  return true;
}

bool BackgroundTracingManagerImpl::HasActiveScenarioForTesting() {
  return config_;
}

void BackgroundTracingManagerImpl::ValidateStartupScenario() {
  if (!config_ || !delegate_)
    return;

  if (!delegate_->IsAllowedToBeginBackgroundScenario(
          *config_.get(), requires_anonymized_data_)) {
    AbortScenario();
  }
}

void BackgroundTracingManagerImpl::EnableRecordingIfConfigNeedsIt() {
  if (!config_)
    return;

  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    EnableRecording(
        GetCategoryFilterStringForCategoryPreset(config_->category_preset()),
        base::trace_event::RECORD_CONTINUOUSLY);
  }
  // There is nothing to do in case of reactive tracing.
}

BackgroundTracingRule*
BackgroundTracingManagerImpl::GetRuleAbleToTriggerTracing(
    TriggerHandle handle) const {
  if (!config_)
    return nullptr;

  // If the last trace is still uploading, we don't allow a new one to trigger.
  if (is_gathering_)
    return nullptr;

  if (!IsTriggerHandleValid(handle)) {
    return nullptr;
  }

  std::string trigger_name = GetTriggerNameFromHandle(handle);
  for (const auto& rule : config_.get()->rules()) {
    if (static_cast<BackgroundTracingRule*>(rule)
            ->ShouldTriggerNamedEvent(trigger_name))
      return static_cast<BackgroundTracingRule*>(rule);
  }

  return nullptr;
}

void BackgroundTracingManagerImpl::TriggerNamedEvent(
    BackgroundTracingManagerImpl::TriggerHandle handle,
    StartedFinalizingCallback callback) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::TriggerNamedEvent,
                   base::Unretained(this), handle, callback));
    return;
  }

  BackgroundTracingRule* triggered_rule = GetRuleAbleToTriggerTracing(handle);
  if (!triggered_rule) {
    if (!callback.is_null())
      callback.Run(false);
    return;
  }

  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    RecordBackgroundTracingMetric(PREEMPTIVE_TRIGGERED);
    BeginFinalizing(callback);
  } else {
    // A different reactive config tried to trigger.
    if (is_tracing_ && handle != reactive_triggered_handle_) {
      if (!callback.is_null())
        callback.Run(false);
      return;
    }

    RecordBackgroundTracingMetric(REACTIVE_TRIGGERED);
    if (is_tracing_) {
      tracing_timer_->CancelTimer();
      BeginFinalizing(callback);
      return;
    }

    // It was not already tracing, start a new trace.
    EnableRecording(GetCategoryFilterStringForCategoryPreset(
                        triggered_rule->GetCategoryPreset()),
                    base::trace_event::RECORD_UNTIL_FULL);
    tracing_timer_.reset(new TracingTimer(callback));
    tracing_timer_->StartTimer(triggered_rule->GetReactiveTimeout());
    reactive_triggered_handle_ = handle;
  }
}

BackgroundTracingManagerImpl::TriggerHandle
BackgroundTracingManagerImpl::RegisterTriggerType(const char* trigger_name) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  trigger_handle_ids_ += 1;

  trigger_handles_.insert(
      std::pair<TriggerHandle, std::string>(trigger_handle_ids_, trigger_name));

  return static_cast<TriggerHandle>(trigger_handle_ids_);
}

bool BackgroundTracingManagerImpl::IsTriggerHandleValid(
    BackgroundTracingManager::TriggerHandle handle) const {
  return trigger_handles_.find(handle) != trigger_handles_.end();
}

std::string BackgroundTracingManagerImpl::GetTriggerNameFromHandle(
    BackgroundTracingManager::TriggerHandle handle) const {
  CHECK(IsTriggerHandleValid(handle));
  return trigger_handles_.find(handle)->second;
}

void BackgroundTracingManagerImpl::InvalidateTriggerHandlesForTesting() {
  trigger_handles_.clear();
}

void BackgroundTracingManagerImpl::SetTracingEnabledCallbackForTesting(
    const base::Closure& callback) {
  tracing_enabled_callback_for_testing_ = callback;
};

void BackgroundTracingManagerImpl::FireTimerForTesting() {
  tracing_timer_->FireTimerForTesting();
}

void BackgroundTracingManagerImpl::EnableRecording(
    std::string category_filter_str,
    base::trace_event::TraceRecordMode record_mode) {
  base::trace_event::TraceConfig trace_config(category_filter_str, record_mode);
  if (requires_anonymized_data_)
    trace_config.EnableArgumentFilter();

  is_tracing_ = TracingController::GetInstance()->EnableRecording(
      trace_config, tracing_enabled_callback_for_testing_);
  RecordBackgroundTracingMetric(RECORDING_ENABLED);
}

void BackgroundTracingManagerImpl::OnFinalizeStarted(
    base::RefCountedString* file_contents) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  RecordBackgroundTracingMetric(FINALIZATION_STARTED);
  UMA_HISTOGRAM_MEMORY_KB("Tracing.Background.FinalizingTraceSizeInKB",
                          file_contents->size() / 1024);

  if (!receive_callback_.is_null()) {
    receive_callback_.Run(
        file_contents, GenerateMetadataDict(),
        base::Bind(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                   base::Unretained(this)));
  }
}

void BackgroundTracingManagerImpl::OnFinalizeComplete() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                   base::Unretained(this)));
    return;
  }

  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  is_gathering_ = false;

  if (!idle_callback_.is_null())
    idle_callback_.Run();

  // Now that a trace has completed, we may need to enable recording again.
  // TODO(oysteine): Retry later if IsAllowedToBeginBackgroundScenario fails.
  if (!delegate_ ||
      delegate_->IsAllowedToBeginBackgroundScenario(
          *config_.get(), requires_anonymized_data_)) {
    EnableRecordingIfConfigNeedsIt();
  } else {
    AbortScenario();
  }

  RecordBackgroundTracingMetric(FINALIZATION_COMPLETE);
}

scoped_ptr<base::DictionaryValue>
BackgroundTracingManagerImpl::GenerateMetadataDict() const {
  // Grab the network type.
  std::string network_type = GetNetworkTypeString();

  // Grab the product version.
  std::string product_version = GetContentClient()->GetProduct();

  // Serialize the config into json.
  scoped_ptr<base::DictionaryValue> config_dict(new base::DictionaryValue());

  config_->IntoDict(config_dict.get());

  scoped_ptr<base::DictionaryValue> metadata_dict(new base::DictionaryValue());
  metadata_dict->Set("config", config_dict.Pass());
  metadata_dict->SetString("network-type", network_type);
  metadata_dict->SetString("product-version", product_version);

  // OS
  metadata_dict->SetString("os-name", base::SysInfo::OperatingSystemName());
  metadata_dict->SetString("os-version",
                           base::SysInfo::OperatingSystemVersion());
  metadata_dict->SetString("os-arch",
                           base::SysInfo::OperatingSystemArchitecture());

  // CPU
  base::CPU cpu;
  metadata_dict->SetInteger("cpu-family", cpu.family());
  metadata_dict->SetInteger("cpu-model", cpu.model());
  metadata_dict->SetInteger("cpu-stepping", cpu.stepping());
  metadata_dict->SetInteger("num-cpus", base::SysInfo::NumberOfProcessors());
  metadata_dict->SetInteger("physical-memory",
                            base::SysInfo::AmountOfPhysicalMemoryMB());

  std::string cpu_brand = cpu.cpu_brand();
  // Workaround for crbug.com/249713.
  // TODO(oysteine): Remove workaround when bug is fixed.
  size_t null_pos = cpu_brand.find('\0');
  if (null_pos != std::string::npos)
    cpu_brand.erase(null_pos);
  metadata_dict->SetString("cpu-brand", cpu_brand);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();

#if !defined(OS_ANDROID)
  metadata_dict->SetInteger("gpu-venid", gpu_info.gpu.vendor_id);
  metadata_dict->SetInteger("gpu-devid", gpu_info.gpu.device_id);
#endif

  metadata_dict->SetString("gpu-driver", gpu_info.driver_version);
  metadata_dict->SetString("gpu-psver", gpu_info.pixel_shader_version);
  metadata_dict->SetString("gpu-vsver", gpu_info.vertex_shader_version);

#if defined(OS_MACOSX)
  metadata_dict->SetString("gpu-glver", gpu_info.gl_version);
#elif defined(OS_POSIX)
  metadata_dict->SetString("gpu-gl-vendor", gpu_info.gl_vendor);
  metadata_dict->SetString("gpu-gl-renderer", gpu_info.gl_renderer);
#endif

  return metadata_dict.Pass();
}

void BackgroundTracingManagerImpl::BeginFinalizing(
    StartedFinalizingCallback callback) {
  is_gathering_ = true;
  is_tracing_ = false;
  reactive_triggered_handle_ = -1;

  bool is_allowed_finalization =
      !delegate_ || (config_ &&
                     delegate_->IsAllowedToEndBackgroundScenario(
                         *config_.get(), requires_anonymized_data_));

  scoped_refptr<TracingControllerImpl::TraceDataSink> trace_data_sink;
  if (is_allowed_finalization) {
    trace_data_sink = content::TracingController::CreateCompressedStringSink(
        content::TracingController::CreateCallbackEndpoint(
            base::Bind(&BackgroundTracingManagerImpl::OnFinalizeStarted,
                       base::Unretained(this))));
    RecordBackgroundTracingMetric(FINALIZATION_ALLOWED);

    if (auto metadata_dict = GenerateMetadataDict()) {
      std::string results;
      if (base::JSONWriter::Write(*metadata_dict.get(), &results))
        trace_data_sink->SetMetadata(results);
    }
  } else {
    RecordBackgroundTracingMetric(FINALIZATION_DISALLOWED);
  }

  content::TracingController::GetInstance()->DisableRecording(trace_data_sink);

  if (!callback.is_null())
    callback.Run(is_allowed_finalization);
}

void BackgroundTracingManagerImpl::AbortScenario() {
  is_tracing_ = false;
  reactive_triggered_handle_ = -1;
  config_.reset();

  content::TracingController::GetInstance()->DisableRecording(nullptr);
}

std::string
BackgroundTracingManagerImpl::GetCategoryFilterStringForCategoryPreset(
    BackgroundTracingConfigImpl::CategoryPreset preset) const {
  switch (preset) {
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK:
      return "benchmark,toplevel";
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_DEEP:
      return "*,disabled-by-default-benchmark.detailed,"
             "disabled-by-default-v8.cpu_profile";
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_GPU:
      return "benchmark,toplevel,gpu";
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_IPC:
      return "benchmark,toplevel,ipc";
    case BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP:
      return "benchmark,toplevel,startup,disabled-by-default-file";
  }
  NOTREACHED();
  return "";
}

}  // namspace content
