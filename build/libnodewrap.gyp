{
  'variables': {
    'conditions': [
      ['OS=="mac"', {
        'LIB_NODE': '<(PRODUCT_DIR)/libnode.a',
      }, {
        'LIB_NODE': '<(PRODUCT_DIR)/nodejs/libnode.a',
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'libnodewrap',
      'type': 'executable',

      'dependencies': [
        '../nodejs/node.gyp:node',
      ],

      'include_dirs': [
        '../nodejs/src',
        '../nodejs/deps/v8/include'
      ],

      'sources': [
        'libnodewrap.cc',
      ],

      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(LIB_NODE)',
              '-Wl,-force_load,<(PRODUCT_DIR)/<(OPENSSL_PRODUCT)',
              '-Wl,-force_load,<(V8_BASE)',
              '-lc++',
            ],
          },
        }],

        ['OS in "linux freebsd"', {
          'ldflags': [
            '-Wl,--whole-archive,'
                '<(LIB_NODE)',
                '<(PRODUCT_DIR)/nodejs/deps/openssl/<(OPENSSL_PRODUCT)',
                '<(PRODUCT_DIR)/nodejs/deps/v8/src/libv8_base.a',
            '-Wl,--no-whole-archive',
            '-lstdc++',
          ],
        }],

        # openssl.def is based on zlib.def, zlib symbols
        # are always exported.
        ['OS=="win"', {
          'sources': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
        }],
      ],
    },
  ],
}
