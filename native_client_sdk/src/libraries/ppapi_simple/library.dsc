{
  'TOOLS': ['bionic', 'glibc', 'pnacl', 'linux', 'mac', 'clang-newlib'],
  'TARGETS': [
    {
      'NAME' : 'ppapi_simple',
      'TYPE' : 'lib',
      'LIBS': ['nacl_io'],
      'DEPS': ['nacl_io'],
      'SOURCES' : [
        "ps.c",
        "ps_context_2d.c",
        "ps_event.c",
        "ps_instance.c",
        "ps_interface.c",
        "ps_main.c",
        "ps_entrypoints_c.c"
      ],
    },
  ],
  'HEADERS': [
    {
      'FILES': [
        "ps.h",
        "ps_context_2d.h",
        "ps_event.h",
        "ps_instance.h",
        "ps_interface.h",
        "ps_internal.h",
        "ps_main.h",
      ],
      'DEST': 'include/ppapi_simple',
    },
  ],
  'DEST': 'src',
  'NAME': 'ppapi_simple',
}
