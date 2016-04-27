'use strict';
require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const depmod = require.resolve('../fixtures/deprecated.js');
const node = process.execPath;

const depUserland =
    require.resolve('../fixtures/deprecated-userland-function.js');

const normal = [depmod];
const noDep = ['--no-deprecation', depmod];
const traceDep = ['--trace-deprecation', depmod];

execFile(node, normal, function(er, stdout, stderr) {
  console.error('normal: show deprecation warning');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/util\.debug is deprecated/.test(stderr));
  console.log('normal ok');
});

execFile(node, noDep, function(er, stdout, stderr) {
  console.error('--no-deprecation: silence deprecations');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert.equal(stderr, 'DEBUG: This is deprecated\n');
  console.log('silent ok');
});

execFile(node, traceDep, function(er, stdout, stderr) {
  console.error('--trace-deprecation: show stack');
  assert.equal(er, null);
  assert.equal(stdout, '');
  var stack = stderr.trim().split('\n');
  // just check the top and bottom.
  assert(/util.debug is deprecated. Use console.error instead./.test(stack[1]));
  assert(/DEBUG: This is deprecated/.test(stack[0]));
  console.log('trace ok');
});

execFile(node, [depUserland], function(er, stdout, stderr) {
  console.error('normal: testing deprecated userland function');
  assert.equal(er, null);
  assert.equal(stdout, '');
  assert(/deprecatedFunction is deprecated/.test(stderr));
  console.error('normal: ok');
});
