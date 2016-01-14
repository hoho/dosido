'use strict';
var common = require('../common');
var assert = require('assert');
var a = require('assert');

function makeBlock(f) {
  var args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return f.apply(this, args);
  };
}

assert.ok(common.indirectInstanceOf(a.AssertionError.prototype, Error),
          'a.AssertionError instanceof Error');

assert.throws(makeBlock(a, false), a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a, true), a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a, 'test', 'ok(\'test\')'));

assert.throws(makeBlock(a.ok, false),
              a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a.ok, true),
                    a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a.ok, 'test'), 'ok(\'test\')');

assert.throws(makeBlock(a.equal, true, false), a.AssertionError, 'equal');

assert.doesNotThrow(makeBlock(a.equal, null, null), 'equal');

assert.doesNotThrow(makeBlock(a.equal, undefined, undefined), 'equal');

assert.doesNotThrow(makeBlock(a.equal, null, undefined), 'equal');

assert.doesNotThrow(makeBlock(a.equal, true, true), 'equal');

assert.doesNotThrow(makeBlock(a.equal, 2, '2'), 'equal');

assert.doesNotThrow(makeBlock(a.notEqual, true, false), 'notEqual');

assert.throws(makeBlock(a.notEqual, true, true),
              a.AssertionError, 'notEqual');

assert.throws(makeBlock(a.strictEqual, 2, '2'),
              a.AssertionError, 'strictEqual');

assert.throws(makeBlock(a.strictEqual, null, undefined),
              a.AssertionError, 'strictEqual');

assert.doesNotThrow(makeBlock(a.notStrictEqual, 2, '2'), 'notStrictEqual');

// deepEquals joy!
// 7.2
assert.doesNotThrow(makeBlock(a.deepEqual, new Date(2000, 3, 14),
                    new Date(2000, 3, 14)), 'deepEqual date');

assert.throws(makeBlock(a.deepEqual, new Date(), new Date(2000, 3, 14)),
              a.AssertionError,
              'deepEqual date');

// 7.3
assert.doesNotThrow(makeBlock(a.deepEqual, /a/, /a/));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/g, /a/g));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/i, /a/i));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/m, /a/m));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/igm, /a/igm));
assert.throws(makeBlock(a.deepEqual, /ab/, /a/));
assert.throws(makeBlock(a.deepEqual, /a/g, /a/));
assert.throws(makeBlock(a.deepEqual, /a/i, /a/));
assert.throws(makeBlock(a.deepEqual, /a/m, /a/));
assert.throws(makeBlock(a.deepEqual, /a/igm, /a/im));

var re1 = /a/;
re1.lastIndex = 3;
assert.throws(makeBlock(a.deepEqual, re1, /a/));


// 7.4
assert.doesNotThrow(makeBlock(a.deepEqual, 4, '4'), 'deepEqual == check');
assert.doesNotThrow(makeBlock(a.deepEqual, true, 1), 'deepEqual == check');
assert.throws(makeBlock(a.deepEqual, 4, '5'),
              a.AssertionError,
              'deepEqual == check');

// 7.5
// having the same number of owned properties && the same set of keys
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4}, {a: 4}));
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4, b: '2'}, {a: 4, b: '2'}));
assert.doesNotThrow(makeBlock(a.deepEqual, [4], ['4']));
assert.throws(makeBlock(a.deepEqual, {a: 4}, {a: 4, b: true}),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, ['a'], {0: 'a'}));
//(although not necessarily the same order),
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4, b: '1'}, {b: '1', a: 4}));
var a1 = [1, 2, 3];
var a2 = [1, 2, 3];
a1.a = 'test';
a1.b = true;
a2.b = true;
a2.a = 'test';
assert.throws(makeBlock(a.deepEqual, Object.keys(a1), Object.keys(a2)),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, a1, a2));

// having an identical prototype property
var nbRoot = {
  toString: function() { return this.first + ' ' + this.last; }
};

function nameBuilder(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder.prototype = nbRoot;

function nameBuilder2(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder2.prototype = nbRoot;

var nb1 = new nameBuilder('Ryan', 'Dahl');
var nb2 = new nameBuilder2('Ryan', 'Dahl');

assert.doesNotThrow(makeBlock(a.deepEqual, nb1, nb2));

nameBuilder2.prototype = Object;
nb2 = new nameBuilder2('Ryan', 'Dahl');
assert.doesNotThrow(makeBlock(a.deepEqual, nb1, nb2));

// primitives and object
assert.throws(makeBlock(a.deepEqual, null, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, undefined, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 'a', ['a']), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 'a', {0: 'a'}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 1, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, true, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, Symbol(), {}), a.AssertionError);

// primitive wrappers and object
assert.doesNotThrow(makeBlock(a.deepEqual, new String('a'), ['a']),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new String('a'), {0: 'a'}),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new Number(1), {}),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new Boolean(true), {}),
                    a.AssertionError);

//deepStrictEqual
assert.doesNotThrow(makeBlock(a.deepStrictEqual, new Date(2000, 3, 14),
                    new Date(2000, 3, 14)), 'deepStrictEqual date');

assert.throws(makeBlock(a.deepStrictEqual, new Date(), new Date(2000, 3, 14)),
              a.AssertionError,
              'deepStrictEqual date');

// 7.3 - strict
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/, /a/));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/g, /a/g));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/i, /a/i));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/m, /a/m));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/igm, /a/igm));
assert.throws(makeBlock(a.deepStrictEqual, /ab/, /a/));
assert.throws(makeBlock(a.deepStrictEqual, /a/g, /a/));
assert.throws(makeBlock(a.deepStrictEqual, /a/i, /a/));
assert.throws(makeBlock(a.deepStrictEqual, /a/m, /a/));
assert.throws(makeBlock(a.deepStrictEqual, /a/igm, /a/im));

var re1 = /a/;
re1.lastIndex = 3;
assert.throws(makeBlock(a.deepStrictEqual, re1, /a/));


// 7.4 - strict
assert.throws(makeBlock(a.deepStrictEqual, 4, '4'),
              a.AssertionError,
              'deepStrictEqual === check');

assert.throws(makeBlock(a.deepStrictEqual, true, 1),
              a.AssertionError,
              'deepStrictEqual === check');

assert.throws(makeBlock(a.deepStrictEqual, 4, '5'),
              a.AssertionError,
              'deepStrictEqual === check');

// 7.5 - strict
// having the same number of owned properties && the same set of keys
assert.doesNotThrow(makeBlock(a.deepStrictEqual, {a: 4}, {a: 4}));
assert.doesNotThrow(makeBlock(a.deepStrictEqual,
                              {a: 4, b: '2'},
                              {a: 4, b: '2'}));
assert.throws(makeBlock(a.deepStrictEqual, [4], ['4']));
assert.throws(makeBlock(a.deepStrictEqual, {a: 4}, {a: 4, b: true}),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, ['a'], {0: 'a'}));
//(although not necessarily the same order),
assert.doesNotThrow(makeBlock(a.deepStrictEqual,
                              {a: 4, b: '1'},
                              {b: '1', a: 4}));

assert.throws(makeBlock(a.deepStrictEqual,
                        [0, 1, 2, 'a', 'b'],
                        [0, 1, 2, 'b', 'a']),
              a.AssertionError);

assert.doesNotThrow(makeBlock(a.deepStrictEqual, a1, a2));

// Prototype check
function Constructor1(first, last) {
  this.first = first;
  this.last = last;
}

function Constructor2(first, last) {
  this.first = first;
  this.last = last;
}

var obj1 = new Constructor1('Ryan', 'Dahl');
var obj2 = new Constructor2('Ryan', 'Dahl');

assert.throws(makeBlock(a.deepStrictEqual, obj1, obj2), a.AssertionError);

Constructor2.prototype = Constructor1.prototype;
obj2 = new Constructor2('Ryan', 'Dahl');

assert.doesNotThrow(makeBlock(a.deepStrictEqual, obj1, obj2));

// primitives
assert.throws(makeBlock(assert.deepStrictEqual, 4, '4'),
              a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, true, 1),
              a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, Symbol(), Symbol()),
              a.AssertionError);

var s = Symbol();
assert.doesNotThrow(makeBlock(assert.deepStrictEqual, s, s));


// primitives and object
assert.throws(makeBlock(a.deepStrictEqual, null, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, undefined, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 'a', ['a']), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 'a', {0: 'a'}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 1, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, true, {}), a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, Symbol(), {}),
              a.AssertionError);


// primitive wrappers and object
assert.throws(makeBlock(a.deepStrictEqual, new String('a'), ['a']),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new String('a'), {0: 'a'}),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new Number(1), {}),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new Boolean(true), {}),
              a.AssertionError);


// Testing the throwing
function thrower(errorConstructor) {
  throw new errorConstructor('test');
}

// the basic calls work
assert.throws(makeBlock(thrower, a.AssertionError),
              a.AssertionError, 'message');
assert.throws(makeBlock(thrower, a.AssertionError), a.AssertionError);
assert.throws(makeBlock(thrower, a.AssertionError));

// if not passing an error, catch all.
assert.throws(makeBlock(thrower, TypeError));

// when passing a type, only catch errors of the appropriate type
var threw = false;
try {
  a.throws(makeBlock(thrower, TypeError), a.AssertionError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof TypeError, 'type');
}
assert.equal(true, threw,
             'a.throws with an explicit error is eating extra errors',
             a.AssertionError);
threw = false;

// doesNotThrow should pass through all errors
try {
  a.doesNotThrow(makeBlock(thrower, TypeError), a.AssertionError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof TypeError);
}
assert.equal(true, threw,
             'a.doesNotThrow with an explicit error is eating extra errors');

// key difference is that throwing our correct error makes an assertion error
try {
  a.doesNotThrow(makeBlock(thrower, TypeError), TypeError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof a.AssertionError);
}
assert.equal(true, threw,
             'a.doesNotThrow is not catching type matching errors');

assert.throws(function() {assert.ifError(new Error('test error'));});
assert.doesNotThrow(function() {assert.ifError(null);});
assert.doesNotThrow(function() {assert.ifError();});

// make sure that validating using constructor really works
threw = false;
try {
  assert.throws(
      function() {
        throw ({});
      },
      Array
  );
} catch (e) {
  threw = true;
}
assert.ok(threw, 'wrong constructor validation');

// use a RegExp to validate error message
a.throws(makeBlock(thrower, TypeError), /test/);

// use a fn to validate error object
a.throws(makeBlock(thrower, TypeError), function(err) {
  if ((err instanceof TypeError) && /test/.test(err)) {
    return true;
  }
});

// https://github.com/nodejs/node/issues/3188
threw = false;

try {
  var ES6Error = class extends Error {};

  var AnotherErrorType = class extends Error {};

  const functionThatThrows = function() {
    throw new AnotherErrorType('foo');
  };

  assert.throws(functionThatThrows, ES6Error);
} catch (e) {
  threw = true;
  assert(e instanceof AnotherErrorType,
    `expected AnotherErrorType, received ${e}`);
}

assert.ok(threw);

// GH-207. Make sure deepEqual doesn't loop forever on circular refs
var b = {};
b.b = b;

var c = {};
c.b = c;

var gotError = false;
try {
  assert.deepEqual(b, c);
} catch (e) {
  gotError = true;
}

// GH-7178. Ensure reflexivity of deepEqual with `arguments` objects.
var args = (function() { return arguments; })();
a.throws(makeBlock(a.deepEqual, [], args));
a.throws(makeBlock(a.deepEqual, args, []));
assert.ok(gotError);


var circular = {y: 1};
circular.x = circular;

function testAssertionMessage(actual, expected) {
  try {
    assert.equal(actual, '');
  } catch (e) {
    assert.equal(e.toString(),
        ['AssertionError:', expected, '==', '\'\''].join(' '));
    assert.ok(e.generatedMessage, 'Message not marked as generated');
  }
}

testAssertionMessage(undefined, 'undefined');
testAssertionMessage(null, 'null');
testAssertionMessage(true, 'true');
testAssertionMessage(false, 'false');
testAssertionMessage(0, '0');
testAssertionMessage(100, '100');
testAssertionMessage(NaN, 'NaN');
testAssertionMessage(Infinity, 'Infinity');
testAssertionMessage(-Infinity, '-Infinity');
testAssertionMessage('', '""');
testAssertionMessage('foo', '\'foo\'');
testAssertionMessage([], '[]');
testAssertionMessage([1, 2, 3], '[ 1, 2, 3 ]');
testAssertionMessage(/a/, '/a/');
testAssertionMessage(/abc/gim, '/abc/gim');
testAssertionMessage(function f() {}, '[Function: f]');
testAssertionMessage(function() {}, '[Function]');
testAssertionMessage({}, '{}');
testAssertionMessage(circular, '{ y: 1, x: [Circular] }');
testAssertionMessage({a: undefined, b: null}, '{ a: undefined, b: null }');
testAssertionMessage({a: NaN, b: Infinity, c: -Infinity},
    '{ a: NaN, b: Infinity, c: -Infinity }');

// #2893
try {
  assert.throws(function() {
    assert.ifError(null);
  });
} catch (e) {
  threw = true;
  assert.equal(e.message, 'Missing expected exception..');
}
assert.ok(threw);

// #5292
try {
  assert.equal(1, 2);
} catch (e) {
  assert.equal(e.toString().split('\n')[0], 'AssertionError: 1 == 2');
  assert.ok(e.generatedMessage, 'Message not marked as generated');
}

try {
  assert.equal(1, 2, 'oh no');
} catch (e) {
  assert.equal(e.toString().split('\n')[0], 'AssertionError: oh no');
  assert.equal(e.generatedMessage, false,
              'Message incorrectly marked as generated');
}

// Verify that throws() and doesNotThrow() throw on non-function block
function testBlockTypeError(method, block) {
  var threw = true;

  try {
    method(block);
    threw = false;
  } catch (e) {
    assert.equal(e.toString(), 'TypeError: block must be a function');
  }

  assert.ok(threw);
}

testBlockTypeError(assert.throws, 'string');
testBlockTypeError(assert.doesNotThrow, 'string');
testBlockTypeError(assert.throws, 1);
testBlockTypeError(assert.doesNotThrow, 1);
testBlockTypeError(assert.throws, true);
testBlockTypeError(assert.doesNotThrow, true);
testBlockTypeError(assert.throws, false);
testBlockTypeError(assert.doesNotThrow, false);
testBlockTypeError(assert.throws, []);
testBlockTypeError(assert.doesNotThrow, []);
testBlockTypeError(assert.throws, {});
testBlockTypeError(assert.doesNotThrow, {});
testBlockTypeError(assert.throws, /foo/);
testBlockTypeError(assert.doesNotThrow, /foo/);
testBlockTypeError(assert.throws, null);
testBlockTypeError(assert.doesNotThrow, null);
testBlockTypeError(assert.throws, undefined);
testBlockTypeError(assert.doesNotThrow, undefined);

// https://github.com/nodejs/node/issues/3275
assert.throws(() => { throw 'error'; }, err => err === 'error');
assert.throws(() => { throw new Error(); }, err => err instanceof Error);

console.log('All OK');
