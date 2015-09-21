# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_resources',
      'type': 'none',
      'dependencies': [
        'checkbox/compiled_resources.gyp:*',
        'internet_page/compiled_resources.gyp:*',
        'languages_page/compiled_resources.gyp:*',
        'on_startup_page/compiled_resources.gyp:*',
        'prefs/compiled_resources.gyp:*',
        'radio_group/compiled_resources.gyp:*',
      ],
    },
  ]
}
