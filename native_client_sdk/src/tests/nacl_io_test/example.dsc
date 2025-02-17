{
  'TOOLS': ['bionic', 'clang-newlib', 'glibc', 'pnacl', 'linux', 'mac'],
  'SEL_LDR': True,

  'TARGETS': [
    {
      'NAME' : 'nacl_io_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'dev_fs_for_testing.h',
        'event_test.cc',
        'fake_ppapi/fake_core_interface.cc',
        'fake_ppapi/fake_core_interface.h',
        'fake_ppapi/fake_host_resolver_interface.cc',
        'fake_ppapi/fake_host_resolver_interface.h',
        'fake_ppapi/fake_messaging_interface.cc',
        'fake_ppapi/fake_messaging_interface.h',
        'fake_ppapi/fake_net_address_interface.cc',
        'fake_ppapi/fake_net_address_interface.h',
        'fake_ppapi/fake_pepper_interface.cc',
        'fake_ppapi/fake_pepper_interface.h',
        'fake_ppapi/fake_pepper_interface_html5_fs.cc',
        'fake_ppapi/fake_pepper_interface_html5_fs.h',
        'fake_ppapi/fake_pepper_interface_url_loader.cc',
        'fake_ppapi/fake_pepper_interface_url_loader.h',
        'fake_ppapi/fake_resource_manager.cc',
        'fake_ppapi/fake_resource_manager.h',
        'fake_ppapi/fake_var_array_buffer_interface.cc',
        'fake_ppapi/fake_var_array_buffer_interface.h',
        'fake_ppapi/fake_var_array_interface.cc',
        'fake_ppapi/fake_var_array_interface.h',
        'fake_ppapi/fake_var_dictionary_interface.cc',
        'fake_ppapi/fake_var_dictionary_interface.h',
        'fake_ppapi/fake_var_interface.cc',
        'fake_ppapi/fake_var_interface.h',
        'fake_ppapi/fake_var_manager.cc',
        'fake_ppapi/fake_var_manager.h',
        'fifo_test.cc',
        'filesystem_test.cc',
        'fuse_fs_test.cc',
        'host_resolver_test.cc',
        'html5_fs_test.cc',
        'http_fs_test.cc',
        'js_fs_test.cc',
        'jspipe_test.cc',
        'kernel_object_test.cc',
        'kernel_proxy_test.cc',
        'kernel_wrap_test.cc',
        'main.cc',
        'mem_fs_node_test.cc',
        'mock_fs.cc',
        'mock_fs.h',
        'mock_kernel_proxy.h',
        'mock_node.cc',
        'mock_node.h',
        'mock_util.h',
        'syscalls_test.cc',
        'path_test.cc',
        'pepper_interface_mock.cc',
        'pepper_interface_mock.h',
        'socket_test.cc',
        'tty_test.cc',
      ],
      'DEPS': ['ppapi_simple', 'nacl_io'],
      # Order matters here: gtest has a "main" function that will be used if
      # referenced before ppapi.
      'LIBS': ['ppapi_simple_cpp', 'ppapi_cpp', 'gmock', 'nacl_io', 'ppapi', 'gtest', 'pthread'],
      'INCLUDES': ["."],
      'CXXFLAGS': ['-Wno-sign-compare'],
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'tests',
  'NAME': 'nacl_io_test',
  'TITLE': 'NaCl IO test',
}
