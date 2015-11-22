/* eslint-disable no-debugger */
// Flags: --expose_internals
'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const Protocol = require('_debugger').Protocol;

if (common.isWindows) {
  console.log('1..0 # Skipped: SCHED_RR not reliable on Windows');
  return;
}

cluster.schedulingPolicy = cluster.SCHED_RR;

// Worker sends back a "I'm here" message, then immediately suspends
// inside the debugger.  The master connects to the debug agent first,
// connects to the TCP server second, then disconnects the worker and
// unsuspends it again.  The ultimate goal of this tortured exercise
// is to make sure the connection is still sitting in the master's
// pending handle queue.
if (cluster.isMaster) {
  const handles = require('internal/cluster').handles;
  // FIXME(bnoordhuis) lib/cluster.js scans the execArgv arguments for
  // debugger flags and renumbers any port numbers it sees starting
  // from the default port 5858.  Add a '.' that circumvents the
  // scanner but is ignored by atoi(3).  Heinous hack.
  cluster.setupMaster({ execArgv: [`--debug=${common.PORT}.`] });
  const worker = cluster.fork();
  worker.on('message', common.mustCall(message => {
    assert.strictEqual(Array.isArray(message), true);
    assert.strictEqual(message[0], 'listening');
    const address = message[1];
    const host = address.address;
    const debugClient = net.connect({ host, port: common.PORT });
    const protocol = new Protocol();
    debugClient.setEncoding('utf8');
    debugClient.on('data', data => protocol.execute(data));
    debugClient.once('connect', common.mustCall(() => {
      protocol.onResponse = common.mustCall(res => {
        protocol.onResponse = () => {};
        const conn = net.connect({ host, port: address.port });
        conn.once('connect', common.mustCall(() => {
          conn.destroy();
          assert.notDeepStrictEqual(handles, {});
          worker.disconnect();
          assert.deepStrictEqual(handles, {});
          const req = protocol.serialize({ command: 'continue' });
          debugClient.write(req);
        }));
      });
    }));
  }));
  process.on('exit', () => assert.deepStrictEqual(handles, {}));
} else {
  const server = net.createServer(socket => socket.pipe(socket));
  server.listen(() => {
    process.send(['listening', server.address()]);
    debugger;
  });
  process.on('disconnect', process.exit);
}
