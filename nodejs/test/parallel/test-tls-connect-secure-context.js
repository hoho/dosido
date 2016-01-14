'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');

const keysDir = path.join(common.fixturesDir, 'keys');

const ca = fs.readFileSync(path.join(keysDir, 'ca1-cert.pem'));
const cert = fs.readFileSync(path.join(keysDir, 'agent1-cert.pem'));
const key = fs.readFileSync(path.join(keysDir, 'agent1-key.pem'));

const server = tls.createServer({
  cert: cert,
  key: key
}, function(c) {
  c.end();
}).listen(common.PORT, function() {
  const secureContext = tls.createSecureContext({
    ca: ca
  });

  const socket = tls.connect({
    secureContext: secureContext,
    servername: 'agent1',
    port: common.PORT
  }, common.mustCall(function() {
    server.close();
    socket.end();
  }));
});
