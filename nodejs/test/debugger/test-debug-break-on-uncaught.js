'use strict';
var path = require('path');
var assert = require('assert');
var spawn = require('child_process').spawn;
var common = require('../common');
var debug = require('_debugger');

addScenario('global.js', null, 2);
addScenario('timeout.js', null, 2);

run();

/***************** IMPLEMENTATION *****************/

var scenarios;
function addScenario(scriptName, throwsInFile, throwsOnLine) {
  if (!scenarios) scenarios = [];
  scenarios.push(
    runScenario.bind(null, scriptName, throwsInFile, throwsOnLine, run)
  );
}

function run() {
  var next = scenarios.shift();
  if (next) next();
}

function runScenario(scriptName, throwsInFile, throwsOnLine, next) {
  console.log('**[ %s ]**', scriptName);
  var asserted = false;
  var port = common.PORT + 1337;

  var testScript = path.join(
    common.fixturesDir,
    'uncaught-exceptions',
    scriptName
  );

  var child = spawn(process.execPath, [ '--debug-brk=' + port, testScript ]);
  child.on('close', function() {
    assert(asserted, 'debugger did not pause on exception');
    if (next) next();
  });

  var exceptions = [];

  setTimeout(setupClient.bind(null, runTest), 200);

  function setupClient(callback) {
    var client = new debug.Client();

    client.once('ready', callback.bind(null, client));

    client.on('unhandledResponse', function(body) {
      console.error('unhandled response: %j', body);
    });

    client.on('error', function(err) {
      if (asserted) return;
      assert.ifError(err);
    });

    client.connect(port);
  }

  function runTest(client) {
    client.req(
      {
        command: 'setexceptionbreak',
        arguments: {
          type: 'uncaught',
          enabled: true
        }
      },
      function(error, result) {
        assert.ifError(error);

        client.on('exception', function(event) {
          exceptions.push(event.body);
        });

        client.reqContinue(function(error, result) {
          assert.ifError(error);
          setTimeout(assertHasPaused.bind(null, client), 100);
        });
      }
    );
  }

  function assertHasPaused(client) {
    assert.equal(exceptions.length, 1, 'debugger did not pause on exception');
    assert.equal(exceptions[0].uncaught, true);
    assert.equal(exceptions[0].script.name, throwsInFile || testScript);
    if (throwsOnLine != null)
      assert.equal(exceptions[0].sourceLine + 1, throwsOnLine);
    asserted = true;
    client.reqContinue(assert.ifError);
  }
}
