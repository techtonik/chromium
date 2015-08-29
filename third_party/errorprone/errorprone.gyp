# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'errorprone_script_path': '<(PRODUCT_DIR)/bin.java/chromium_errorprone',
  },
  'targets': [
    {
      # GN: //third_party/errorprone:error_prone_annotation_java
      'target_name': 'error_prone_annotation_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/error_prone_annotation-2.0.1.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/errorprone:error_prone_annotations_java
      'target_name': 'error_prone_annotations_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/error_prone_annotations-2.0.1.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/errorprone:javacutil_java
      'target_name': 'javacutil_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/javacutil-1.8.10.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/errorprone:dataflow_java
      'target_name': 'dataflow_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/dataflow-1.8.10.jar',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ]
    },
    {
      # GN: //third_party/errorprone:chromium_errorprone
      'target_name': 'chromium_errorprone',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/error_prone_core-2.0.1.jar',
      },
      'dependencies': [
        '../../build/android/setup.gyp:sun_tools_java',
        '../../third_party/findbugs/findbugs.gyp:format_string_jar',
        'error_prone_annotation_jar',
        'error_prone_annotations_jar',
        'javacutil_jar',
        'dataflow_jar',
      ],
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ],
      'actions': [
        {
          'action_name': 'create_errorprone_binary_script',
          'inputs': [
            '<(DEPTH)/build/android/gyp/create_java_binary_script.py',
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            # Ensure that the script is touched when the jar is.
            '<(jar_path)',
          ],
          'outputs': [
            '<(errorprone_script_path)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/create_java_binary_script.py',
            '--output', '<(PRODUCT_DIR)/bin.java/chromium_errorprone',
            '--jar-path=<(jar_path)',
            '--classpath=>@(input_jars_paths)',
            '--main-class=com.google.errorprone.ErrorProneCompiler',
          ],
        },
      ],
    },
    {
      # This emulates gn's datadeps fields. We don't want the errorprone jars
      # to be added to the classpaths of targets that depend on errorprone.
      'target_name': 'require_errorprone',
      'type': 'none',
      'actions': [
        {
          'action_name': 'require_errorprone',
          'message': 'Making sure errorprone has been built.',
          'variables': {
            'required_file': '<(PRODUCT_DIR)/bin.java/errorprone.required',
          },
          'inputs': [
            '<(errorprone_script_path)',
          ],
          'outputs': [
            '<(required_file)',
          ],
          'action': [
            'python', '../../build/android/gyp/touch.py', '<(required_file)',
          ],
        },
      ],
    },
  ],
}
