// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_

#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/common/content_export.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {

class CONTENT_EXPORT BackgroundTracingRule {
 public:
  BackgroundTracingRule();
  virtual ~BackgroundTracingRule();

  virtual void Install() {}
  virtual void IntoDict(base::DictionaryValue* dict) const = 0;
  virtual bool ShouldTriggerNamedEvent(const std::string& named_event) const;
  virtual BackgroundTracingConfigImpl::CategoryPreset GetCategoryPreset() const;
  virtual void OnHistogramTrigger(const std::string& histogram_name) const {}

  virtual int GetReactiveTimeout() const;

  static scoped_ptr<BackgroundTracingRule> PreemptiveRuleFromDict(
      const base::DictionaryValue* dict);

  static scoped_ptr<BackgroundTracingRule> ReactiveRuleFromDict(
      const base::DictionaryValue* dict,
      BackgroundTracingConfigImpl::CategoryPreset category_preset);

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingRule);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_RULE_H_
