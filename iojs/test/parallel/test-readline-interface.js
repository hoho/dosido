'use strict';
var assert = require('assert');
var readline = require('readline');
var EventEmitter = require('events').EventEmitter;
var inherits = require('util').inherits;

function FakeInput() {
  EventEmitter.call(this);
}
inherits(FakeInput, EventEmitter);
FakeInput.prototype.resume = function() {};
FakeInput.prototype.pause = function() {};
FakeInput.prototype.write = function() {};
FakeInput.prototype.end = function() {};

function isWarned(emitter) {
  for (var name in emitter) {
    var listeners = emitter[name];
    if (listeners.warned) return true;
  }
  return false;
}

[ true, false ].forEach(function(terminal) {
  var fi;
  var rli;
  var called;

  // sending a full line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);

  // sending a blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, '');
  });
  fi.emit('data', '\n');
  assert.ok(called);

  // sending a single character with no newline
  fi = new FakeInput();
  rli = new readline.Interface(fi, {});
  called = false;
  rli.on('line', function(line) {
    called = true;
  });
  fi.emit('data', 'a');
  assert.ok(!called);
  rli.close();

  // sending a single character with no newline and then a newline
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'a');
  });
  fi.emit('data', 'a');
  assert.ok(!called);
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // sending multiple newlines at once
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  var expectedLines = ['foo', 'bar', 'baz'];
  var callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n') + '\n');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();

  // sending multiple newlines at once that does not end with a new(empty)
  // line and a `end` event
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', ''];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  rli.on('close', function() {
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  fi.emit('end');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  // and a `end` event(last line is)

  // \r\n should emit one line event, not two
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r\n'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();

  // \r\n should emit one line event when split across multiple writes.
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  expectedLines.forEach(function(line) {
    fi.emit('data', line + '\r');
    fi.emit('data', '\n');
  });
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // \r should behave like \n when alone
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();

  // \r at start of input should output blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['', 'foo' ];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', '\rfoo\r');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // sending a multi-byte utf8 char over multiple writes
  var buf = Buffer('☮', 'utf8');
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  callCount = 0;
  rli.on('line', function(line) {
    callCount++;
    assert.equal(line, buf.toString('utf8'));
  });
  [].forEach.call(buf, function(i) {
    fi.emit('data', Buffer([i]));
  });
  assert.equal(callCount, 0);
  fi.emit('data', '\n');
  assert.equal(callCount, 1);
  rli.close();

  // calling readline without `new`
  fi = new FakeInput();
  rli = readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);
  rli.close();

  if (terminal) {
    // question
    fi = new FakeInput();
    rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
    expectedLines = ['foo'];
    rli.question(expectedLines[0], function() {
      rli.close();
    });
    var cursorPos = rli._getCursorPos();
    assert.equal(cursorPos.rows, 0);
    assert.equal(cursorPos.cols, expectedLines[0].length);
    rli.close();

    // sending a multi-line question
    fi = new FakeInput();
    rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
    expectedLines = ['foo', 'bar'];
    rli.question(expectedLines.join('\n'), function() {
      rli.close();
    });
    var cursorPos = rli._getCursorPos();
    assert.equal(cursorPos.rows, expectedLines.length - 1);
    assert.equal(cursorPos.cols, expectedLines.slice(-1)[0].length);
    rli.close();
  }

  // wide characters should be treated as two columns.
  assert.equal(readline.isFullWidthCodePoint('a'.charCodeAt(0)), false);
  assert.equal(readline.isFullWidthCodePoint('あ'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint('谢'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint('고'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint(0x1f251), true); // surrogate
  assert.equal(readline.codePointAt('ABC', 0), 0x41);
  assert.equal(readline.codePointAt('あいう', 1), 0x3044);
  assert.equal(readline.codePointAt('\ud800\udc00', 0),  // surrogate
      0x10000);
  assert.equal(readline.codePointAt('\ud800\udc00A', 2), // surrogate
      0x41);
  assert.equal(readline.getStringWidth('abcde'), 5);
  assert.equal(readline.getStringWidth('古池や'), 6);
  assert.equal(readline.getStringWidth('ノード.js'), 9);
  assert.equal(readline.getStringWidth('你好'), 4);
  assert.equal(readline.getStringWidth('안녕하세요'), 10);
  assert.equal(readline.getStringWidth('A\ud83c\ude00BC'), 5); // surrogate

  // check if vt control chars are stripped
  assert.equal(readline
               .stripVTControlCharacters('\u001b[31m> \u001b[39m'), '> ');
  assert.equal(readline
               .stripVTControlCharacters('\u001b[31m> \u001b[39m> '), '> > ');
  assert.equal(readline
               .stripVTControlCharacters('\u001b[31m\u001b[39m'), '');
  assert.equal(readline
               .stripVTControlCharacters('> '), '> ');
  assert.equal(readline.getStringWidth('\u001b[31m> \u001b[39m'), 2);
  assert.equal(readline.getStringWidth('\u001b[31m> \u001b[39m> '), 4);
  assert.equal(readline.getStringWidth('\u001b[31m\u001b[39m'), 0);
  assert.equal(readline.getStringWidth('> '), 2);

  assert.deepEqual(fi.listeners(terminal ? 'keypress' : 'data'), []);

  // check EventEmitter memory leak
  for (var i = 0; i < 12; i++) {
    var rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    rl.close();
    assert.equal(isWarned(process.stdin._events), false);
    assert.equal(isWarned(process.stdout._events), false);
  }

  //can create a new readline Interface with a null output arugument
  fi = new FakeInput();
  rli = new readline.Interface({input: fi, output: null, terminal: terminal });

  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);

  assert.doesNotThrow(function() {
    rli.setPrompt('ddd> ');
  });

  assert.doesNotThrow(function() {
    rli.prompt();
  });

  assert.doesNotThrow(function() {
    rli.write('really shouldnt be seeing this');
  });

  assert.doesNotThrow(function() {
    rli.question('What do you think of node.js? ', function(answer) {
      console.log('Thank you for your valuable feedback:', answer);
      rli.close();
    });
  });

});

