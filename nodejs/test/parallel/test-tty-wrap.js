'use strict';
require('../common');
var assert = require('assert');

var TTY = process.binding('tty_wrap').TTY;
var isTTY = process.binding('tty_wrap').isTTY;

if (isTTY(1) == false) {
  console.log('1..0 # Skipped: fd 1 is not a tty.');
  return;
}

var handle = new TTY(1);
var callbacks = 0;

var req1 = handle.writeBuffer(Buffer.from('hello world\n'));
req1.oncomplete = function() {
  callbacks++;
};

var req2 = handle.writeBuffer(Buffer.from('hello world\n'));
req2.oncomplete = function() {
  callbacks++;
};

process.on('exit', function() {
  assert.equal(2, callbacks);
});
