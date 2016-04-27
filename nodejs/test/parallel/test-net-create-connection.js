'use strict';
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');

const tcpPort = common.PORT;
const expectedConnections = 7;
var clientConnected = 0;
var serverConnected = 0;

const server = net.createServer(function(socket) {
  socket.end();
  if (++serverConnected === expectedConnections) {
    server.close();
  }
});

server.listen(tcpPort, 'localhost', function() {
  function cb() {
    ++clientConnected;
  }

  function fail(opts, errtype, msg) {
    assert.throws(function() {
      net.createConnection(opts, cb);
    }, function(err) {
      return err instanceof errtype && msg === err.message;
    });
  }

  net.createConnection(tcpPort).on('connect', cb);
  net.createConnection(tcpPort, 'localhost').on('connect', cb);
  net.createConnection(tcpPort, cb);
  net.createConnection(tcpPort, 'localhost', cb);
  net.createConnection(tcpPort + '', 'localhost', cb);
  net.createConnection({port: tcpPort + ''}).on('connect', cb);
  net.createConnection({port: '0x' + tcpPort.toString(16)}, cb);

  fail({
    port: true
  }, TypeError, '"port" option should be a number or string: true');

  fail({
    port: false
  }, TypeError, '"port" option should be a number or string: false');

  fail({
    port: []
  }, TypeError, '"port" option should be a number or string: ');

  fail({
    port: {}
  }, TypeError, '"port" option should be a number or string: [object Object]');

  fail({
    port: null
  }, TypeError, '"port" option should be a number or string: null');

  fail({
    port: ''
  }, RangeError, '"port" option should be >= 0 and < 65536: ');

  fail({
    port: ' '
  }, RangeError, '"port" option should be >= 0 and < 65536:  ');

  fail({
    port: '0x'
  }, RangeError, '"port" option should be >= 0 and < 65536: 0x');

  fail({
    port: '-0x1'
  }, RangeError, '"port" option should be >= 0 and < 65536: -0x1');

  fail({
    port: NaN
  }, RangeError, '"port" option should be >= 0 and < 65536: NaN');

  fail({
    port: Infinity
  }, RangeError, '"port" option should be >= 0 and < 65536: Infinity');

  fail({
    port: -1
  }, RangeError, '"port" option should be >= 0 and < 65536: -1');

  fail({
    port: 65536
  }, RangeError, '"port" option should be >= 0 and < 65536: 65536');

  fail({
    hints: (dns.ADDRCONFIG | dns.V4MAPPED) + 42,
  }, TypeError, 'Invalid argument: hints must use valid flags');
});

// Try connecting to random ports, but do so once the server is closed
server.on('close', function() {
  function nop() {}

  net.createConnection({port: 0}).on('error', nop);
  net.createConnection({port: undefined}).on('error', nop);
});

process.on('exit', function() {
  assert.equal(clientConnected, expectedConnections);
});
