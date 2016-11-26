{
  'variables': {
    'conditions': [
          ['GENERATOR == "ninja" or OS== "mac"', {
            'LIB_NODE': '<(PRODUCT_DIR)/libnode.a',
          }, {
            'LIB_NODE': '<(PRODUCT_DIR)/obj.target/libnode.a',
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
        [ '"true"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-force_load,<(LIB_NODE)',
              '-Wl,-force_load,<(PRODUCT_DIR)/<(OPENSSL_PRODUCT)',
              '-Wl,-force_load,<(V8_BASE)',
            ],
          },
          'conditions': [
            ['OS in "linux freebsd" and node_shared=="false"', {
              'ldflags': [
                '-Wl,--whole-archive,'
                    '<(LIB_NODE)',
                    '<(PRODUCT_DIR)/obj.target/deps/openssl/'
                    '<(OPENSSL_PRODUCT)',
                    '<(V8_BASE)'
                '-Wl,--no-whole-archive',
                '-lstdc++'
              ],
            }],
            # openssl.def is based on zlib.def, zlib symbols
            # are always exported.
            ['OS=="win"', {
              'sources': ['<(SHARED_INTERMEDIATE_DIR)/openssl.def'],
            }],
          ],
        }],
      ],
    },
  ],
}
