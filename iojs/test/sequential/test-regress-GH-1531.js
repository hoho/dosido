if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var https = require('https');
var assert = require('assert');
var fs = require('fs');
var common = require('../common');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var gotCallback = false;

var server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

server.listen(common.PORT, function() {
  console.error('listening');
  https.get({
    agent: false,
    path: '/',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    console.error(res.statusCode, res.headers);
    gotCallback = true;
    res.resume();
    server.close();
  }).on('error', function(e) {
    console.error(e.stack);
    process.exit(1);
  });
});

process.on('exit', function() {
  assert.ok(gotCallback);
  console.log('ok');
});

