// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/api/debugger/debugger_api.h"

#include <map>
#include <set>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"

using content::DevToolsAgentHost;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace keys = debugger_api_constants;
namespace Attach = extensions::api::debugger::Attach;
namespace Detach = extensions::api::debugger::Detach;
namespace OnDetach = extensions::api::debugger::OnDetach;
namespace OnEvent = extensions::api::debugger::OnEvent;
namespace SendCommand = extensions::api::debugger::SendCommand;

namespace extensions {
class ExtensionRegistry;
class ExtensionDevToolsClientHost;

namespace {

// Helpers --------------------------------------------------------------------

void CopyDebuggee(Debuggee* dst, const Debuggee& src) {
  if (src.tab_id)
    dst->tab_id.reset(new int(*src.tab_id));
  if (src.extension_id)
    dst->extension_id.reset(new std::string(*src.extension_id));
  if (src.target_id)
    dst->target_id.reset(new std::string(*src.target_id));
}


// ExtensionDevToolsInfoBarDelegate -------------------------------------------

class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an extension dev tools infobar and delegate and adds the infobar to
  // the |infobar_service|. Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   const base::Closure& dismissed_callback,
                                   const std::string& client_name);

 private:
  ExtensionDevToolsInfoBarDelegate(const base::Closure& dismissed_callback,
                                   const std::string& client_name);
  ~ExtensionDevToolsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool Cancel() override;

  std::string client_name_;
  base::Closure dismissed_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsInfoBarDelegate);
};

// static
infobars::InfoBar* ExtensionDevToolsInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const base::Closure& dismissed_callback,
    const std::string& client_name) {
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new ExtensionDevToolsInfoBarDelegate(
          dismissed_callback, client_name))));
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    const base::Closure& dismissed_callback,
    const std::string& client_name)
    : ConfirmInfoBarDelegate(),
      client_name_(client_name),
      dismissed_callback_(dismissed_callback) {}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
}

infobars::InfoBarDelegate::Type
ExtensionDevToolsInfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void ExtensionDevToolsInfoBarDelegate::InfoBarDismissed() {
  DCHECK(!dismissed_callback_.is_null());
  // Use ResetAndReturn() since running the callback may delete |this|.
  base::ResetAndReturn(&dismissed_callback_).Run();
}

base::string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                    base::UTF8ToUTF16(client_name_));
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

bool ExtensionDevToolsInfoBarDelegate::Cancel() {
  InfoBarDismissed();
  // InfoBarDismissed() will have closed us already.
  return false;
}

// GlobalConfirmInfoBar -------------------------------------------------------

// GlobalConfirmInfobar is shown for every tab in every browser until it
// is dismissed or the object itself is destroyed.
// It listens to all tabs in all browsers and adds/removes confirm infobar
// to each of the tabs.
class GlobalConfirmInfoBar : public TabStripModelObserver,
                             public infobars::InfoBarManager::Observer {
 public:
  GlobalConfirmInfoBar(const base::Closure& dismissed_callback,
                       const std::string& client_name);
  ~GlobalConfirmInfoBar() override;

 private:
  using InfoBarMap = std::map<InfoBarService*, infobars::InfoBar*>;

  // TabStripModelObserver:
  void TabInsertedAt(content::WebContents* web_contents,
                     int index,
                     bool foreground) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;
  // We don't override OnInfoBarReplaces, as extension debugger api infobar
  // should not be involved in replacements.

  base::Closure dismissed_callback_;
  std::string client_name_;
  InfoBarMap infobars_;
  BrowserTabStripTracker browser_tab_strip_tracker_;

  DISALLOW_COPY_AND_ASSIGN(GlobalConfirmInfoBar);
};

GlobalConfirmInfoBar::GlobalConfirmInfoBar(
    const base::Closure& dismissed_callback,
    const std::string& client_name)
    : dismissed_callback_(dismissed_callback),
      client_name_(client_name),
      browser_tab_strip_tracker_(this, nullptr, nullptr) {
  browser_tab_strip_tracker_.Init(
      BrowserTabStripTracker::InitWith::BROWSERS_IN_ACTIVE_DESKTOP);
}

GlobalConfirmInfoBar::~GlobalConfirmInfoBar() {
  while (!infobars_.empty()) {
    InfoBarMap::iterator it = infobars_.begin();
    it->first->RemoveInfoBar(it->second);
  }
}

void GlobalConfirmInfoBar::TabInsertedAt(content::WebContents* web_contents,
                                         int index,
                                         bool foreground) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // WebContents from the tab strip must have the infobar service.
  DCHECK(infobar_service);

  infobars::InfoBar* infobar = ExtensionDevToolsInfoBarDelegate::Create(
      infobar_service, dismissed_callback_, client_name_);
  // Infobar with the same delegate won't be added again, so it's safe
  // to not listen to TabReplacedAt.
  if (infobar) {
    infobars_[infobar_service] = infobar;
    infobar_service->AddObserver(this);
  }
}

void GlobalConfirmInfoBar::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  // Generally, our infobars should not be removed externally and we wouldn't
  // need OnInfoBarRemoved. But during browser shutdown all infobars are removed
  // before this class gets a chance to remove infobars itself.
  InfoBarMap::iterator it =
      std::find_if(infobars_.begin(), infobars_.end(),
                   [&infobar](const InfoBarMap::value_type& value) {
                     return value.second == infobar;
                   });
  if (it != infobars_.end())
    OnManagerShuttingDown(it->first);
}

void GlobalConfirmInfoBar::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  InfoBarService* infobar_service = static_cast<InfoBarService*>(manager);
  infobar_service->RemoveObserver(this);
  InfoBarMap::iterator it = infobars_.find(infobar_service);
  DCHECK(it != infobars_.end());
  infobars_.erase(it);
}

}  // namespace

// ExtensionDevToolsClientHost ------------------------------------------------

using AttachedClientHosts = std::set<ExtensionDevToolsClientHost*>;
base::LazyInstance<AttachedClientHosts>::Leaky g_attached_client_hosts =
    LAZY_INSTANCE_INITIALIZER;

class ExtensionDevToolsClientHost : public content::DevToolsAgentHostClient,
                                    public content::NotificationObserver,
                                    public ExtensionRegistryObserver {
 public:
  ExtensionDevToolsClientHost(Profile* profile,
                              DevToolsAgentHost* agent_host,
                              const std::string& extension_id,
                              const std::string& extension_name,
                              const Debuggee& debuggee);

  ~ExtensionDevToolsClientHost() override;

  const std::string& extension_id() { return extension_id_; }
  DevToolsAgentHost* agent_host() { return agent_host_.get(); }
  void Close();
  void SendMessageToBackend(DebuggerSendCommandFunction* function,
                            const std::string& method,
                            SendCommand::Params::CommandParams* command_params);

  // Closes connection as terminated by the user.
  void InfoBarDismissed();

  // DevToolsAgentHostClient interface.
  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;

 private:
  using PendingRequests =
      std::map<int, scoped_refptr<DebuggerSendCommandFunction>>;

  void SendDetachedEvent();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  Profile* profile_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string extension_id_;
  Debuggee debuggee_;
  content::NotificationRegistrar registrar_;
  int last_request_id_;
  PendingRequests pending_requests_;
  scoped_ptr<GlobalConfirmInfoBar> infobar_;
  api::debugger::DetachReason detach_reason_;

  // Listen to extension unloaded notification.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

// ExtensionDevToolsClientHost ------------------------------------------------

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    Profile* profile,
    DevToolsAgentHost* agent_host,
    const std::string& extension_id,
    const std::string& extension_name,
    const Debuggee& debuggee)
    : profile_(profile),
      agent_host_(agent_host),
      extension_id_(extension_id),
      last_request_id_(0),
      detach_reason_(api::debugger::DETACH_REASON_TARGET_CLOSED),
      extension_registry_observer_(this) {
  CopyDebuggee(&debuggee_, debuggee);

  g_attached_client_hosts.Get().insert(this);

  // ExtensionRegistryObserver listen extension unloaded and detach debugger
  // from there.
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));

  // RVH-based agents disconnect from their clients when the app is terminating
  // but shared worker-based agents do not.
  // Disconnect explicitly to make sure that |this| observer is not leaked.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Attach to debugger and tell it we are ready.
  agent_host_->AttachClient(this);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSilentDebuggerExtensionAPI)) {
    // This class owns the |infobar_|, so it's safe to pass Unretained(this).
    infobar_.reset(new GlobalConfirmInfoBar(
        base::Bind(&ExtensionDevToolsClientHost::InfoBarDismissed,
                   base::Unretained(this)),
        extension_name));
  }
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  g_attached_client_hosts.Get().erase(this);
}

// DevToolsAgentHostClient implementation.
void ExtensionDevToolsClientHost::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced_with_another_client) {
  DCHECK(agent_host == agent_host_.get());
  if (replaced_with_another_client)
    detach_reason_ = api::debugger::DETACH_REASON_REPLACED_WITH_DEVTOOLS;
  SendDetachedEvent();
  delete this;
}

void ExtensionDevToolsClientHost::Close() {
  agent_host_->DetachClient();
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    DebuggerSendCommandFunction* function,
    const std::string& method,
    SendCommand::Params::CommandParams* command_params) {
  base::DictionaryValue protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.SetInteger("id", request_id);
  protocol_request.SetString("method", method);
  if (command_params) {
    protocol_request.Set("params",
                         command_params->additional_properties.DeepCopy());
  }

  std::string json_args;
  base::JSONWriter::Write(protocol_request, &json_args);
  agent_host_->DispatchProtocolMessage(json_args);
}

void ExtensionDevToolsClientHost::InfoBarDismissed() {
  detach_reason_ = api::debugger::DETACH_REASON_CANCELED_BY_USER;
  SendDetachedEvent();
  Close();
}

void ExtensionDevToolsClientHost::SendDetachedEvent() {
  if (!EventRouter::Get(profile_))
    return;

  scoped_ptr<base::ListValue> args(OnDetach::Create(debuggee_,
                                                    detach_reason_));
  scoped_ptr<Event> event(
      new Event(events::DEBUGGER_ON_DETACH, OnDetach::kEventName, args.Pass()));
  event->restrict_to_browser_context = profile_;
  EventRouter::Get(profile_)
      ->DispatchEventToExtension(extension_id_, event.Pass());
}

void ExtensionDevToolsClientHost::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (extension->id() == extension_id_)
    Close();
}

void ExtensionDevToolsClientHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  Close();
}

void ExtensionDevToolsClientHost::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {
  DCHECK(agent_host == agent_host_.get());
  if (!EventRouter::Get(profile_))
    return;

  scoped_ptr<base::Value> result = base::JSONReader::Read(message);
  if (!result->IsType(base::Value::TYPE_DICTIONARY))
    return;
  base::DictionaryValue* dictionary =
      static_cast<base::DictionaryValue*>(result.get());

  int id;
  if (!dictionary->GetInteger("id", &id)) {
    std::string method_name;
    if (!dictionary->GetString("method", &method_name))
      return;

    OnEvent::Params params;
    base::DictionaryValue* params_value;
    if (dictionary->GetDictionary("params", &params_value))
      params.additional_properties.Swap(params_value);

    scoped_ptr<base::ListValue> args(
        OnEvent::Create(debuggee_, method_name, params));
    scoped_ptr<Event> event(
        new Event(events::DEBUGGER_ON_EVENT, OnEvent::kEventName, args.Pass()));
    event->restrict_to_browser_context = profile_;
    EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, event.Pass());
  } else {
    DebuggerSendCommandFunction* function = pending_requests_[id].get();
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}


// DebuggerFunction -----------------------------------------------------------

DebuggerFunction::DebuggerFunction()
    : client_host_(NULL) {
}

DebuggerFunction::~DebuggerFunction() {
}

void DebuggerFunction::FormatErrorMessage(const std::string& format) {
  if (debuggee_.tab_id)
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kTabTargetType, base::IntToString(*debuggee_.tab_id));
  else if (debuggee_.extension_id)
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kBackgroundPageTargetType, *debuggee_.extension_id);
  else
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kOpaqueTargetType, *debuggee_.target_id);
}

bool DebuggerFunction::InitAgentHost() {
  if (debuggee_.tab_id) {
    WebContents* web_contents = NULL;
    bool result = ExtensionTabUtil::GetTabById(*debuggee_.tab_id,
                                               GetProfile(),
                                               include_incognito(),
                                               NULL,
                                               NULL,
                                               &web_contents,
                                               NULL);
    if (result && web_contents) {
      // TODO(rdevlin.cronin) This should definitely be GetLastCommittedURL().
      GURL url = web_contents->GetVisibleURL();
      if (PermissionsData::IsRestrictedUrl(url, extension(), &error_))
        return false;
      agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
    }
  } else if (debuggee_.extension_id) {
    ExtensionHost* extension_host =
        ProcessManager::Get(GetProfile())
            ->GetBackgroundHostForExtension(*debuggee_.extension_id);
    if (extension_host) {
      if (PermissionsData::IsRestrictedUrl(extension_host->GetURL(),
                                           extension(),
                                           &error_)) {
        return false;
      }
      agent_host_ =
          DevToolsAgentHost::GetOrCreateFor(extension_host->host_contents());
    }
  } else if (debuggee_.target_id) {
    agent_host_ = DevToolsAgentHost::GetForId(*debuggee_.target_id);
    if (agent_host_.get()) {
      if (PermissionsData::IsRestrictedUrl(agent_host_->GetURL(),
                                           extension(),
                                           &error_)) {
        agent_host_ = nullptr;
        return false;
      }
    }
  } else {
    error_ = keys::kInvalidTargetError;
    return false;
  }

  if (!agent_host_.get()) {
    FormatErrorMessage(keys::kNoTargetError);
    return false;
  }
  return true;
}

bool DebuggerFunction::InitClientHost() {
  if (!InitAgentHost())
    return false;

  const std::string& extension_id = extension()->id();
  DevToolsAgentHost* agent_host = agent_host_.get();
  AttachedClientHosts& hosts = g_attached_client_hosts.Get();
  AttachedClientHosts::iterator it = std::find_if(
      hosts.begin(), hosts.end(),
      [&agent_host, &extension_id](ExtensionDevToolsClientHost* client_host) {
        return client_host->agent_host() == agent_host &&
               client_host->extension_id() == extension_id;
      });

  if (it == hosts.end()) {
    FormatErrorMessage(keys::kNotAttachedError);
    return false;
  }

  client_host_ = *it;
  return true;
}


// DebuggerAttachFunction -----------------------------------------------------

DebuggerAttachFunction::DebuggerAttachFunction() {
}

DebuggerAttachFunction::~DebuggerAttachFunction() {
}

bool DebuggerAttachFunction::RunAsync() {
  scoped_ptr<Attach::Params> params(Attach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitAgentHost())
    return false;

  if (!DevToolsAgentHost::IsSupportedProtocolVersion(
          params->required_version)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kProtocolVersionNotSupportedError,
        params->required_version);
    return false;
  }

  if (agent_host_->IsAttached()) {
    FormatErrorMessage(keys::kAlreadyAttachedError);
    return false;
  }

  new ExtensionDevToolsClientHost(GetProfile(), agent_host_.get(),
                                  extension()->id(), extension()->name(),
                                  debuggee_);
  SendResponse(true);
  return true;
}


// DebuggerDetachFunction -----------------------------------------------------

DebuggerDetachFunction::DebuggerDetachFunction() {
}

DebuggerDetachFunction::~DebuggerDetachFunction() {
}

bool DebuggerDetachFunction::RunAsync() {
  scoped_ptr<Detach::Params> params(Detach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->Close();
  SendResponse(true);
  return true;
}


// DebuggerSendCommandFunction ------------------------------------------------

DebuggerSendCommandFunction::DebuggerSendCommandFunction() {
}

DebuggerSendCommandFunction::~DebuggerSendCommandFunction() {
}

bool DebuggerSendCommandFunction::RunAsync() {
  scoped_ptr<SendCommand::Params> params(SendCommand::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(&debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->SendMessageToBackend(this, params->method,
      params->command_params.get());
  return true;
}

void DebuggerSendCommandFunction::SendResponseBody(
    base::DictionaryValue* response) {
  base::Value* error_body;
  if (response->Get("error", &error_body)) {
    base::JSONWriter::Write(*error_body, &error_);
    SendResponse(false);
    return;
  }

  base::DictionaryValue* result_body;
  SendCommand::Results::Result result;
  if (response->GetDictionary("result", &result_body))
    result.additional_properties.Swap(result_body);

  results_ = SendCommand::Results::Create(result);
  SendResponse(true);
}


// DebuggerGetTargetsFunction -------------------------------------------------

namespace {

const char kTargetIdField[] = "id";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetAttachedField[] = "attached";
const char kTargetUrlField[] = "url";
const char kTargetFaviconUrlField[] = "faviconUrl";
const char kTargetTypePage[] = "page";
const char kTargetTypeBackgroundPage[] = "background_page";
const char kTargetTypeWorker[] = "worker";
const char kTargetTypeOther[] = "other";
const char kTargetTabIdField[] = "tabId";
const char kTargetExtensionIdField[] = "extensionId";

base::Value* SerializeTarget(const DevToolsTargetImpl& target) {
  base::DictionaryValue* dictionary = new base::DictionaryValue();

  dictionary->SetString(kTargetIdField, target.GetId());
  dictionary->SetString(kTargetTitleField, target.GetTitle());
  dictionary->SetBoolean(kTargetAttachedField, target.IsAttached());
  dictionary->SetString(kTargetUrlField, target.GetURL().spec());

  std::string type = target.GetType();
  if (type == kTargetTypePage) {
    dictionary->SetInteger(kTargetTabIdField, target.GetTabId());
  } else if (type == kTargetTypeBackgroundPage) {
    dictionary->SetString(kTargetExtensionIdField, target.GetExtensionId());
  } else if (type != kTargetTypeWorker) {
    // DevToolsTargetImpl may support more types than the debugger API.
    type = kTargetTypeOther;
  }
  dictionary->SetString(kTargetTypeField, type);

  GURL favicon_url = target.GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary->SetString(kTargetFaviconUrlField, favicon_url.spec());

  return dictionary;
}

}  // namespace

DebuggerGetTargetsFunction::DebuggerGetTargetsFunction() {
}

DebuggerGetTargetsFunction::~DebuggerGetTargetsFunction() {
}

bool DebuggerGetTargetsFunction::RunAsync() {
  std::vector<DevToolsTargetImpl*> list = DevToolsTargetImpl::EnumerateAll();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DebuggerGetTargetsFunction::SendTargetList, this, list));
  return true;
}

void DebuggerGetTargetsFunction::SendTargetList(
    const std::vector<DevToolsTargetImpl*>& target_list) {
  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (size_t i = 0; i < target_list.size(); ++i)
    result->Append(SerializeTarget(*target_list[i]));
  STLDeleteContainerPointers(target_list.begin(), target_list.end());
  SetResult(result.release());
  SendResponse(true);
}

}  // namespace extensions
