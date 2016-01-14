'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

// neither should hang
tls.createSecurePair(null, false, false, false);
tls.createSecurePair(null, true, false, false);
