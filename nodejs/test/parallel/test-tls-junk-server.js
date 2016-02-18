'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');
const net = require('net');

const server = net.createServer(function(s) {
  s.once('data', function() {
    s.end('I was waiting for you, hello!', function() {
      s.destroy();
    });
  });
});

server.listen(common.PORT, function() {
  const req = https.request({ port: common.PORT });
  req.end();

  req.once('error', common.mustCall(function(err) {
    assert(/unknown protocol/.test(err.message));
    server.close();
  }));
});
