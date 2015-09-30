'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

if (cluster.isWorker) {

  // keep the worker alive
  var http = require('http');
  http.Server().listen(common.PORT, '127.0.0.1');

} else if (process.argv[2] === 'cluster') {

  var worker = cluster.fork();

  // send PID info to testcase process
  process.send({
    pid: worker.process.pid
  });

  // terminate the cluster process
  worker.once('listening', function() {
    setTimeout(function() {
      process.exit(0);
    }, 1000);
  });

} else {

  // This is the testcase
  var fork = require('child_process').fork;

  // is process alive helper
  var isAlive = function(pid) {
    try {
      //this will throw an error if the process is dead
      process.kill(pid, 0);

      return true;
    } catch (e) {
      return false;
    }
  };

  // Spawn a cluster process
  var master = fork(process.argv[1], ['cluster']);

  // get pid info
  var pid = null;
  master.once('message', function(data) {
    pid = data.pid;
  });

  // When master is dead
  var alive = true;
  master.on('exit', function(code) {

    // make sure that the master died by purpose
    assert.equal(code, 0);

    // check worker process status
    var timeout = 200;
    if (common.isAix) {
      // AIX needs more time due to default exit performance
      timeout = 1000;
    }
    setTimeout(function() {
      alive = isAlive(pid);
    }, timeout);
  });

  process.once('exit', function() {
    // cleanup: kill the worker if alive
    if (alive) {
      process.kill(pid);
    }

    assert.equal(typeof pid, 'number', 'did not get worker pid info');
    assert.equal(alive, false, 'worker was alive after master died');
  });

}
