'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const async_wrap = process.binding('async_wrap');
const providers = Object.keys(async_wrap.Providers);

const uidSymbol = Symbol('uid');

let cntr = 0;
let server;
let client;

function init(uid, type, parentUid, parentHandle) {
  this[uidSymbol] = uid;

  if (parentHandle) {
    cntr++;
    // Cannot assert in init callback or will abort.
    process.nextTick(() => {
      assert.equal(providers[type], 'TCPWRAP');
      assert.equal(parentUid, server._handle[uidSymbol],
                   'server uid doesn\'t match parent uid');
      assert.equal(parentHandle, server._handle,
                   'server handle doesn\'t match parent handle');
      assert.equal(this, client._handle, 'client doesn\'t match context');
    });
  }
}

function noop() { }

async_wrap.setupHooks(init, noop, noop);
async_wrap.enable();

server = net.createServer(function(c) {
  client = c;
  // Allow init callback to run before closing.
  setImmediate(() => {
    c.end();
    this.close();
  });
}).listen(common.PORT, function() {
  net.connect(common.PORT, noop);
});

async_wrap.disable();

process.on('exit', function() {
  // init should have only been called once with a parent.
  assert.equal(cntr, 1);
});
