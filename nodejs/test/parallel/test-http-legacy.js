'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

function p(x) {
  common.error(common.inspect(x));
}

var responses_sent = 0;
var responses_recvd = 0;
var body0 = '';
var body1 = '';

var server = http.createServer(function(req, res) {
  if (responses_sent == 0) {
    assert.equal('GET', req.method);
    assert.equal('/hello', url.parse(req.url).pathname);

    console.dir(req.headers);
    assert.equal(true, 'accept' in req.headers);
    assert.equal('*/*', req.headers['accept']);

    assert.equal(true, 'foo' in req.headers);
    assert.equal('bar', req.headers['foo']);
  }

  if (responses_sent == 1) {
    assert.equal('POST', req.method);
    assert.equal('/world', url.parse(req.url).pathname);
    this.close();
  }

  req.on('end', function() {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('The path was ' + url.parse(req.url).pathname);
    res.end();
    responses_sent += 1;
  });
  req.resume();

  //assert.equal('127.0.0.1', res.connection.remoteAddress);
});

server.listen(common.PORT, function() {
  var client = http.createClient(common.PORT);
  var req = client.request('/hello', {'Accept': '*/*', 'Foo': 'bar'});
  setTimeout(function() {
    req.end();
  }, 100);
  req.on('response', function(res) {
    assert.equal(200, res.statusCode);
    responses_recvd += 1;
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body0 += chunk; });
    common.debug('Got /hello response');
  });

  setTimeout(function() {
    var req = client.request('POST', '/world');
    req.end();
    req.on('response', function(res) {
      assert.equal(200, res.statusCode);
      responses_recvd += 1;
      res.setEncoding('utf8');
      res.on('data', function(chunk) { body1 += chunk; });
      common.debug('Got /world response');
    });
  }, 1);
});

process.on('exit', function() {
  common.debug('responses_recvd: ' + responses_recvd);
  assert.equal(2, responses_recvd);

  common.debug('responses_sent: ' + responses_sent);
  assert.equal(2, responses_sent);

  assert.equal('The path was /hello', body0);
  assert.equal('The path was /world', body1);
});
