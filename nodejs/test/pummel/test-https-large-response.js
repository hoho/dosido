'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqCount = 0;

process.stdout.write('build body...');
var body = 'hello world\n'.repeat(1024 * 1024);
process.stdout.write('done\n');

var server = https.createServer(options, function(req, res) {
  reqCount++;
  console.log('got request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});

var count = 0;
var gotResEnd = false;

server.listen(common.PORT, function() {
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    console.log('response!');

    res.on('data', function(d) {
      process.stdout.write('.');
      count += d.length;
      res.pause();
      process.nextTick(function() {
        res.resume();
      });
    });

    res.on('end', function(d) {
      process.stdout.write('\n');
      console.log('expected: ', body.length);
      console.log('     got: ', count);
      server.close();
      gotResEnd = true;
    });
  });
});


process.on('exit', function() {
  assert.equal(1, reqCount);
  assert.equal(body.length, count);
  assert.ok(gotResEnd);
});
