'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');
const fs = require('fs');
const constants = require('constants');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  secureOptions: constants.SSL_OP_NO_TICKET
};

// Create TLS1.2 server
https.createServer(options, function(req, res) {
  res.end('ohai');
}).listen(common.PORT, function() {
  first(this);
});

// Do request and let agent cache the session
function first(server)  {
  const req = https.request({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();

    server.close(function() {
      faultyServer();
    });
  });
  req.end();
}

// Create TLS1 server
function faultyServer() {
  options.secureProtocol = 'TLSv1_method';
  https.createServer(options, function(req, res) {
    res.end('hello faulty');
  }).listen(common.PORT, function() {
    second(this);
  });
}

// Attempt to request using cached session
function second(server, session) {
  const req = https.request({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();
  });

  // Let it fail
  req.on('error', common.mustCall(function(err) {
    assert(/wrong version number/.test(err.message));

    req.on('close', function() {
      third(server);
    });
  }));
  req.end();
}

// Try on more time - session should be evicted!
function third(server) {
  const req = https.request({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.resume();
    assert(!req.socket.isSessionReused());
    server.close();
  });
  req.on('error', function(err) {
    // never called
    assert(false);
  });
  req.end();
}
