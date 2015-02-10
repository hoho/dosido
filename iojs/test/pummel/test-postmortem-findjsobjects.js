var common = require('../common');
var assert = require('assert');
var os = require('os');
var path = require('path');
var util = require('util');

if (os.type() != 'SunOS') {
  console.error('Skipping because postmortem debugging not available.');
  process.exit(0);
}

/*
 * Now we're going to fork ourselves to gcore
 */
var spawn = require('child_process').spawn;
var prefix = '/var/tmp/node';
var corefile = prefix + '.' + process.pid;
var gcore = spawn('gcore', [ '-o', prefix, process.pid + '' ]);
var output = '';
var unlinkSync = require('fs').unlinkSync;
var args = [ corefile ];

if (process.env.MDB_LIBRARY_PATH && process.env.MDB_LIBRARY_PATH != '')
  args = args.concat([ '-L', process.env.MDB_LIBRARY_PATH ]);

function LanguageH(chapter) { this.OBEY = 'CHAPTER ' + parseInt(chapter, 10); }
var obj = new LanguageH(1);

gcore.stderr.on('data', function (data) {
  console.log('gcore: ' + data);
});

gcore.on('exit', function (code) {
  if (code != 0) {
    console.error('gcore exited with code ' + code);
    process.exit(code);
  }

  var mdb = spawn('mdb', args, { stdio: 'pipe' });

  mdb.on('exit', function (code) {
    var retained = '; core retained as ' + corefile;

    if (code != 0) {
      console.error('mdb exited with code ' + util.inspect(code) + retained);
      process.exit(code);
    }

    var lines = output.split('\n');
    var found = 0, i, expected = 'OBEY: "' + obj.OBEY + '"', nexpected = 2;

    for (var i = 0; i < lines.length; i++) {
      if (lines[i].indexOf(expected) != -1)
        found++;
    }

    assert.equal(found, nexpected, 'expected ' + nexpected +
      ' objects, found ' + found + retained);

    unlinkSync(corefile);
    process.exit(0);
  });

  mdb.stdout.on('data', function (data) {
    output += data;
  });

  mdb.stderr.on('data', function (data) {
    console.log('mdb stderr: ' + data);
  });

  var mod = util.format('::load %s\n',
                        path.join(__dirname,
                                  '..',
                                  '..',
                                  'out',
                                  'Release',
                                  'mdb_v8.so'));
  mdb.stdin.write(mod);
  mdb.stdin.write('::findjsobjects -c LanguageH | ');
  mdb.stdin.write('::findjsobjects | ::jsprint\n');
  mdb.stdin.write('::findjsobjects -p OBEY | ');
  mdb.stdin.write('::findjsobjects | ::jsprint\n');
  mdb.stdin.end();
});

