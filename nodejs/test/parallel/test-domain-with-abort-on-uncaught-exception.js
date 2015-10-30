'use strict';

const assert = require('assert');
const fs = require('fs');
const common = require('../common');

/*
 * The goal of this test is to make sure that:
 *
 * - Even if --abort_on_uncaught_exception is passed on the command line,
 * setting up a top-level domain error handler and throwing an error
 * within this domain does *not* make the process abort. The process exits
 * gracefully.
 *
 * - When passing --abort_on_uncaught_exception on the command line and
 * setting up a top-level domain error handler, an error thrown
 * within this domain's error handler *does* make the process abort.
 *
 * - When *not* passing --abort_on_uncaught_exception on the command line and
 * setting up a top-level domain error handler, an error thrown within this
 * domain's error handler does *not* make the process abort, but makes it exit
 * with the proper failure exit code.
 *
 * - When throwing an error within the top-level domain's error handler
 * within a try/catch block, the process should exit gracefully, whether or
 * not --abort_on_uncaught_exception is passed on the command line.
 */

const domainErrHandlerExMessage = 'exception from domain error handler';

if (process.argv[2] === 'child') {
  var domain = require('domain');
  var d = domain.create();
  var triggeredProcessUncaughtException = false;

  process.on('uncaughtException', function onUncaughtException() {
    // The process' uncaughtException event must not be emitted when
    // an error handler is setup on the top-level domain.
    // Exiting with exit code of 42 here so that it would assert when
    // the parent checks the child exit code.
    process.exit(42);
  });

  d.on('error', function(err) {
    // Swallowing the error on purpose if 'throwInDomainErrHandler' is not
    // set
    if (process.argv.indexOf('throwInDomainErrHandler') !== -1) {
      // If useTryCatch is set, wrap the throw in a try/catch block.
      // This is to make sure that a caught exception does not trigger
      // an abort.
      if (process.argv.indexOf('useTryCatch') !== -1) {
        try {
          throw new Error(domainErrHandlerExMessage);
        } catch (e) {
        }
      } else {
        throw new Error(domainErrHandlerExMessage);
      }
    }
  });

  d.run(function doStuff() {
    // Throwing from within different types of callbacks as each of them
    // handles domains differently
    process.nextTick(function() {
      throw new Error('Error from nextTick callback');
    });

    fs.exists('/non/existing/file', function onExists(exists) {
      throw new Error('Error from fs.exists callback');
    });

    setImmediate(function onSetImmediate() {
      throw new Error('Error from setImmediate callback');
    });

    setTimeout(function onTimeout() {
      throw new Error('Error from setTimeout callback');
    }, 0);

    throw new Error('Error from domain.run callback');
  });
} else {
  var exec = require('child_process').exec;

  function testDomainExceptionHandling(cmdLineOption, options) {
    if (typeof cmdLineOption === 'object') {
      options = cmdLineOption;
      cmdLineOption = undefined;
    }

    var throwInDomainErrHandlerOpt;
    if (options.throwInDomainErrHandler)
      throwInDomainErrHandlerOpt = 'throwInDomainErrHandler';

    var cmdToExec = '';
    if (process.platform !== 'win32') {
      // Do not create core files, as it can take a lot of disk space on
      // continuous testing and developers' machines
      cmdToExec += 'ulimit -c 0 && ';
    }

    var useTryCatchOpt;
    if (options.useTryCatch)
      useTryCatchOpt = 'useTryCatch';

    cmdToExec +=  process.argv[0] + ' ';
    cmdToExec += (cmdLineOption ? cmdLineOption : '') + ' ';
    cmdToExec += process.argv[1] + ' ';
    cmdToExec += [
      'child',
      throwInDomainErrHandlerOpt,
      useTryCatchOpt
    ].join(' ');

    var child = exec(cmdToExec);

    if (child) {
      var childTriggeredOnUncaughtExceptionHandler = false;
      child.on('message', function onChildMsg(msg) {
        if (msg === 'triggeredProcessUncaughtEx') {
          childTriggeredOnUncaughtExceptionHandler = true;
        }
      });

      child.on('exit', function onChildExited(exitCode, signal) {
        var expectedExitCodes;
        var expectedSignals;

        // When throwing errors from the top-level domain error handler
        // outside of a try/catch block, the process should not exit gracefully
        if (!options.useTryCatch && options.throwInDomainErrHandler) {
          if (cmdLineOption === '--abort_on_uncaught_exception') {
            // If the top-level domain's error handler throws, and only if
            // --abort_on_uncaught_exception is passed on the command line,
            // the process must abort.
            //
            // Depending on the compiler used, node will exit with either
            // exit code 132 (SIGILL), 133 (SIGTRAP) or 134 (SIGABRT).
            expectedExitCodes = [132, 133, 134];

            // On platforms using KSH as the default shell (like SmartOS),
            // when a process aborts, KSH exits with an exit code that is
            // greater than 256, and thus the exit code emitted with the 'exit'
            // event is null and the signal is set to either SIGILL, SIGTRAP,
            // or SIGABRT (depending on the compiler).
            expectedSignals = ['SIGILL', 'SIGTRAP', 'SIGABRT'];

            // On Windows, v8's base::OS::Abort triggers an access violation,
            // which corresponds to exit code 3221225477 (0xC0000005)
            if (process.platform === 'win32')
              expectedExitCodes = [3221225477];

            // When using --abort-on-uncaught-exception, V8 will use
            // base::OS::Abort to terminate the process.
            // Depending on the compiler used, the shell or other aspects of
            // the platform used to build the node binary, this will actually
            // make V8 exit by aborting or by raising a signal. In any case,
            // one of them (exit code or signal) needs to be set to one of
            // the expected exit codes or signals.
            if (signal !== null) {
              assert.ok(expectedSignals.indexOf(signal) > -1);
            } else {
              assert.ok(expectedExitCodes.indexOf(exitCode) > -1);
            }
          } else {
            // By default, uncaught exceptions make node exit with an exit
            // code of 7.
            assert.equal(exitCode, 7);
            assert.equal(signal, null);
          }
        } else {
          // If the top-level domain's error handler does not throw,
          // the process must exit gracefully, whether or not
          // --abort_on_uncaught_exception was passed on the command line
          assert.equal(exitCode, 0);
          assert.equal(signal, null);
        }
      });
    }
  }

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: false,
                              useTryCatch: false
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: false,
                              useTryCatch: true
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: true,
                              useTryCatch: false
                            });

  testDomainExceptionHandling('--abort_on_uncaught_exception', {
                              throwInDomainErrHandler: true,
                              useTryCatch: true
                            });

  testDomainExceptionHandling({
    throwInDomainErrHandler: false
  });

  testDomainExceptionHandling({
    throwInDomainErrHandler: false,
    useTryCatch: false
  });

  testDomainExceptionHandling({
    throwInDomainErrHandler: true,
    useTryCatch: true
  });
}
