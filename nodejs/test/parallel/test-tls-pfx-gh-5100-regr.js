'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: node compiled without crypto.');
  return;
}

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');
const path = require('path');

const pfx = fs.readFileSync(
    path.join(common.fixturesDir, 'keys', 'agent1-pfx.pem'));

const server = tls.createServer({
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
}, common.mustCall(function(c) {
  assert(c.authorizationError === null, 'authorizationError must be null');
  c.end();
})).listen(common.PORT, function() {
  var client = tls.connect({
    port: common.PORT,
    pfx: pfx,
    passphrase: 'sample',
    rejectUnauthorized: false
  }, function() {
    client.end();
    server.close();
  });
});
