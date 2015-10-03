'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

http.createServer(function(req, res) {
  req.resume();
  req.on('end', function() {
    write(res);
  });
  this.close();
}).listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    method: 'PUT'
  });
  write(req);
  req.on('response', function(res) {
    res.resume();
  });
});

var buf = new Buffer(1024 * 16);
buf.fill('x');
function write(out) {
  var name = out.constructor.name;
  var finishEvent = false;
  var endCb = false;

  // first, write until it gets some backpressure
  while (out.write(buf)) {}

  // now end, and make sure that we don't get the 'finish' event
  // before the tick where the cb gets called.  We give it until
  // nextTick because this is added as a listener before the endcb
  // is registered.  The order is not what we're testing here, just
  // that 'finish' isn't emitted until the stream is fully flushed.
  out.on('finish', function() {
    finishEvent = true;
    console.error('%s finish event', name);
    process.nextTick(function() {
      assert(endCb, name + ' got finish event before endcb!');
      console.log('ok - %s finishEvent', name);
    });
  });

  out.end(buf, function() {
    endCb = true;
    console.error('%s endCb', name);
    process.nextTick(function() {
      assert(finishEvent, name + ' got endCb event before finishEvent!');
      console.log('ok - %s endCb', name);
    });
  });
}
