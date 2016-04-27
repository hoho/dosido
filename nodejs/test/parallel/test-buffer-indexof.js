'use strict';
require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer;

var b = Buffer.from('abcdef');
var buf_a = Buffer.from('a');
var buf_bc = Buffer.from('bc');
var buf_f = Buffer.from('f');
var buf_z = Buffer.from('z');
var buf_empty = Buffer.from('');

assert.equal(b.indexOf('a'), 0);
assert.equal(b.indexOf('a', 1), -1);
assert.equal(b.indexOf('a', -1), -1);
assert.equal(b.indexOf('a', -4), -1);
assert.equal(b.indexOf('a', -b.length), 0);
assert.equal(b.indexOf('a', NaN), 0);
assert.equal(b.indexOf('a', -Infinity), 0);
assert.equal(b.indexOf('a', Infinity), -1);
assert.equal(b.indexOf('bc'), 1);
assert.equal(b.indexOf('bc', 2), -1);
assert.equal(b.indexOf('bc', -1), -1);
assert.equal(b.indexOf('bc', -3), -1);
assert.equal(b.indexOf('bc', -5), 1);
assert.equal(b.indexOf('bc', NaN), 1);
assert.equal(b.indexOf('bc', -Infinity), 1);
assert.equal(b.indexOf('bc', Infinity), -1);
assert.equal(b.indexOf('f'), b.length - 1);
assert.equal(b.indexOf('z'), -1);
assert.equal(b.indexOf(''), -1);
assert.equal(b.indexOf('', 1), -1);
assert.equal(b.indexOf('', b.length + 1), -1);
assert.equal(b.indexOf('', Infinity), -1);
assert.equal(b.indexOf(buf_a), 0);
assert.equal(b.indexOf(buf_a, 1), -1);
assert.equal(b.indexOf(buf_a, -1), -1);
assert.equal(b.indexOf(buf_a, -4), -1);
assert.equal(b.indexOf(buf_a, -b.length), 0);
assert.equal(b.indexOf(buf_a, NaN), 0);
assert.equal(b.indexOf(buf_a, -Infinity), 0);
assert.equal(b.indexOf(buf_a, Infinity), -1);
assert.equal(b.indexOf(buf_bc), 1);
assert.equal(b.indexOf(buf_bc, 2), -1);
assert.equal(b.indexOf(buf_bc, -1), -1);
assert.equal(b.indexOf(buf_bc, -3), -1);
assert.equal(b.indexOf(buf_bc, -5), 1);
assert.equal(b.indexOf(buf_bc, NaN), 1);
assert.equal(b.indexOf(buf_bc, -Infinity), 1);
assert.equal(b.indexOf(buf_bc, Infinity), -1);
assert.equal(b.indexOf(buf_f), b.length - 1);
assert.equal(b.indexOf(buf_z), -1);
assert.equal(b.indexOf(buf_empty), -1);
assert.equal(b.indexOf(buf_empty, 1), -1);
assert.equal(b.indexOf(buf_empty, b.length + 1), -1);
assert.equal(b.indexOf(buf_empty, Infinity), -1);
assert.equal(b.indexOf(0x61), 0);
assert.equal(b.indexOf(0x61, 1), -1);
assert.equal(b.indexOf(0x61, -1), -1);
assert.equal(b.indexOf(0x61, -4), -1);
assert.equal(b.indexOf(0x61, -b.length), 0);
assert.equal(b.indexOf(0x61, NaN), 0);
assert.equal(b.indexOf(0x61, -Infinity), 0);
assert.equal(b.indexOf(0x61, Infinity), -1);
assert.equal(b.indexOf(0x0), -1);

// test offsets
assert.equal(b.indexOf('d', 2), 3);
assert.equal(b.indexOf('f', 5), 5);
assert.equal(b.indexOf('f', -1), 5);
assert.equal(b.indexOf('f', 6), -1);

assert.equal(b.indexOf(Buffer.from('d'), 2), 3);
assert.equal(b.indexOf(Buffer.from('f'), 5), 5);
assert.equal(b.indexOf(Buffer.from('f'), -1), 5);
assert.equal(b.indexOf(Buffer.from('f'), 6), -1);

assert.equal(Buffer.from('ff').indexOf(Buffer.from('f'), 1, 'ucs2'), -1);

// test hex encoding
assert.equal(
    Buffer.from(b.toString('hex'), 'hex')
    .indexOf('64', 0, 'hex'), 3);
assert.equal(
    Buffer.from(b.toString('hex'), 'hex')
    .indexOf(Buffer.from('64', 'hex'), 0, 'hex'), 3);

// test base64 encoding
assert.equal(
    Buffer.from(b.toString('base64'), 'base64')
    .indexOf('ZA==', 0, 'base64'), 3);
assert.equal(
    Buffer.from(b.toString('base64'), 'base64')
    .indexOf(Buffer.from('ZA==', 'base64'), 0, 'base64'), 3);

// test ascii encoding
assert.equal(
    Buffer.from(b.toString('ascii'), 'ascii')
    .indexOf('d', 0, 'ascii'), 3);
assert.equal(
    Buffer.from(b.toString('ascii'), 'ascii')
    .indexOf(Buffer.from('d', 'ascii'), 0, 'ascii'), 3);

// test binary encoding
assert.equal(
    Buffer.from(b.toString('binary'), 'binary')
    .indexOf('d', 0, 'binary'), 3);
assert.equal(
    Buffer.from(b.toString('binary'), 'binary')
    .indexOf(Buffer.from('d', 'binary'), 0, 'binary'), 3);
assert.equal(
    Buffer.from('aa\u00e8aa', 'binary')
    .indexOf('\u00e8', 'binary'), 2);
assert.equal(
    Buffer.from('\u00e8', 'binary')
    .indexOf('\u00e8', 'binary'), 0);
assert.equal(
    Buffer.from('\u00e8', 'binary')
    .indexOf(Buffer.from('\u00e8', 'binary'), 'binary'), 0);


// test optional offset with passed encoding
assert.equal(Buffer.from('aaaa0').indexOf('30', 'hex'), 4);
assert.equal(Buffer.from('aaaa00a').indexOf('3030', 'hex'), 4);

{
  // test usc2 encoding
  const twoByteString = Buffer.from('\u039a\u0391\u03a3\u03a3\u0395', 'ucs2');

  assert.equal(8, twoByteString.indexOf('\u0395', 4, 'ucs2'));
  assert.equal(6, twoByteString.indexOf('\u03a3', -4, 'ucs2'));
  assert.equal(4, twoByteString.indexOf('\u03a3', -6, 'ucs2'));
  assert.equal(4, twoByteString.indexOf(
    Buffer.from('\u03a3', 'ucs2'), -6, 'ucs2'));
  assert.equal(-1, twoByteString.indexOf('\u03a3', -2, 'ucs2'));
}

var mixedByteStringUcs2 =
    Buffer.from('\u039a\u0391abc\u03a3\u03a3\u0395', 'ucs2');
assert.equal(6, mixedByteStringUcs2.indexOf('bc', 0, 'ucs2'));
assert.equal(10, mixedByteStringUcs2.indexOf('\u03a3', 0, 'ucs2'));
assert.equal(-1, mixedByteStringUcs2.indexOf('\u0396', 0, 'ucs2'));

assert.equal(
    6, mixedByteStringUcs2.indexOf(Buffer.from('bc', 'ucs2'), 0, 'ucs2'));
assert.equal(
    10, mixedByteStringUcs2.indexOf(Buffer.from('\u03a3', 'ucs2'), 0, 'ucs2'));
assert.equal(
    -1, mixedByteStringUcs2.indexOf(Buffer.from('\u0396', 'ucs2'), 0, 'ucs2'));

{
  const twoByteString = Buffer.from('\u039a\u0391\u03a3\u03a3\u0395', 'ucs2');

  // Test single char pattern
  assert.equal(0, twoByteString.indexOf('\u039a', 0, 'ucs2'));
  assert.equal(2, twoByteString.indexOf('\u0391', 0, 'ucs2'), 'Alpha');
  assert.equal(4, twoByteString.indexOf('\u03a3', 0, 'ucs2'), 'First Sigma');
  assert.equal(6, twoByteString.indexOf('\u03a3', 6, 'ucs2'), 'Second Sigma');
  assert.equal(8, twoByteString.indexOf('\u0395', 0, 'ucs2'), 'Epsilon');
  assert.equal(-1, twoByteString.indexOf('\u0392', 0, 'ucs2'), 'Not beta');

  // Test multi-char pattern
  assert.equal(
      0, twoByteString.indexOf('\u039a\u0391', 0, 'ucs2'), 'Lambda Alpha');
  assert.equal(
      2, twoByteString.indexOf('\u0391\u03a3', 0, 'ucs2'), 'Alpha Sigma');
  assert.equal(
      4, twoByteString.indexOf('\u03a3\u03a3', 0, 'ucs2'), 'Sigma Sigma');
  assert.equal(
      6, twoByteString.indexOf('\u03a3\u0395', 0, 'ucs2'), 'Sigma Epsilon');
}

var mixedByteStringUtf8 = Buffer.from('\u039a\u0391abc\u03a3\u03a3\u0395');
assert.equal(5, mixedByteStringUtf8.indexOf('bc'));
assert.equal(5, mixedByteStringUtf8.indexOf('bc', 5));
assert.equal(5, mixedByteStringUtf8.indexOf('bc', -8));
assert.equal(7, mixedByteStringUtf8.indexOf('\u03a3'));
assert.equal(-1, mixedByteStringUtf8.indexOf('\u0396'));


// Test complex string indexOf algorithms. Only trigger for long strings.
// Long string that isn't a simple repeat of a shorter string.
var longString = 'A';
for (let i = 66; i < 76; i++) {  // from 'B' to 'K'
  longString =  longString + String.fromCharCode(i) + longString;
}

var longBufferString = Buffer.from(longString);

// pattern of 15 chars, repeated every 16 chars in long
var pattern = 'ABACABADABACABA';
for (let i = 0; i < longBufferString.length - pattern.length; i += 7) {
  const index = longBufferString.indexOf(pattern, i);
  assert.equal((i + 15) & ~0xf, index, 'Long ABACABA...-string at index ' + i);
}
assert.equal(510, longBufferString.indexOf('AJABACA'), 'Long AJABACA, First J');
assert.equal(
    1534, longBufferString.indexOf('AJABACA', 511), 'Long AJABACA, Second J');

pattern = 'JABACABADABACABA';
assert.equal(
    511, longBufferString.indexOf(pattern), 'Long JABACABA..., First J');
assert.equal(
    1535, longBufferString.indexOf(pattern, 512), 'Long JABACABA..., Second J');

// Search for a non-ASCII string in a pure ASCII string.
var asciiString = Buffer.from(
    'arglebargleglopglyfarglebargleglopglyfarglebargleglopglyf');
assert.equal(-1, asciiString.indexOf('\x2061'));
assert.equal(3, asciiString.indexOf('leb', 0));

// Search in string containing many non-ASCII chars.
var allCodePoints = [];
for (let i = 0; i < 65536; i++) allCodePoints[i] = i;
var allCharsString = String.fromCharCode.apply(String, allCodePoints);
var allCharsBufferUtf8 = Buffer.from(allCharsString);
var allCharsBufferUcs2 = Buffer.from(allCharsString, 'ucs2');

// Search for string long enough to trigger complex search with ASCII pattern
// and UC16 subject.
assert.equal(-1, allCharsBufferUtf8.indexOf('notfound'));
assert.equal(-1, allCharsBufferUcs2.indexOf('notfound'));

{
  // Find substrings in Utf8.
  const lengths = [1, 3, 15];  // Single char, simple and complex.
  const indices = [0x5, 0x60, 0x400, 0x680, 0x7ee, 0xFF02, 0x16610, 0x2f77b];
  for (let lengthIndex = 0; lengthIndex < lengths.length; lengthIndex++) {
    for (let i = 0; i < indices.length; i++) {
      const index = indices[i];
      let length = lengths[lengthIndex];

      if (index + length > 0x7F) {
        length = 2 * length;
      }

      if (index + length > 0x7FF) {
        length = 3 * length;
      }

      if (index + length > 0xFFFF) {
        length = 4 * length;
      }

      var patternBufferUtf8 = allCharsBufferUtf8.slice(index, index + length);
      assert.equal(index, allCharsBufferUtf8.indexOf(patternBufferUtf8));

      var patternStringUtf8 = patternBufferUtf8.toString();
      assert.equal(index, allCharsBufferUtf8.indexOf(patternStringUtf8));
    }
  }
}

{
  // Find substrings in Usc2.
  const lengths = [2, 4, 16];  // Single char, simple and complex.
  const indices = [0x5, 0x65, 0x105, 0x205, 0x285, 0x2005, 0x2085, 0xfff0];
  for (let lengthIndex = 0; lengthIndex < lengths.length; lengthIndex++) {
    for (let i = 0; i < indices.length; i++) {
      const index = indices[i] * 2;
      const length = lengths[lengthIndex];

      var patternBufferUcs2 =
          allCharsBufferUcs2.slice(index, index + length);
      assert.equal(
          index, allCharsBufferUcs2.indexOf(patternBufferUcs2, 0, 'ucs2'));

      var patternStringUcs2 = patternBufferUcs2.toString('ucs2');
      assert.equal(
          index, allCharsBufferUcs2.indexOf(patternStringUcs2, 0, 'ucs2'));
    }
  }
}

assert.throws(function() {
  b.indexOf(function() { });
});
assert.throws(function() {
  b.indexOf({});
});
assert.throws(function() {
  b.indexOf([]);
});

// All code for handling encodings is shared between Buffer.indexOf and
// Buffer.lastIndexOf, so only testing the separate lastIndexOf semantics.

// Test lastIndexOf basic functionality; Buffer b contains 'abcdef'.
// lastIndexOf string:
assert.equal(b.lastIndexOf('a'), 0);
assert.equal(b.lastIndexOf('a', 1), 0);
assert.equal(b.lastIndexOf('b', 1), 1);
assert.equal(b.lastIndexOf('c', 1), -1);
assert.equal(b.lastIndexOf('a', -1), 0);
assert.equal(b.lastIndexOf('a', -4), 0);
assert.equal(b.lastIndexOf('a', -b.length), 0);
assert.equal(b.lastIndexOf('a', -b.length - 1), -1);
assert.equal(b.lastIndexOf('a', NaN), 0);
assert.equal(b.lastIndexOf('a', -Infinity), -1);
assert.equal(b.lastIndexOf('a', Infinity), 0);
// lastIndexOf Buffer:
assert.equal(b.lastIndexOf(buf_a), 0);
assert.equal(b.lastIndexOf(buf_a, 1), 0);
assert.equal(b.lastIndexOf(buf_a, -1), 0);
assert.equal(b.lastIndexOf(buf_a, -4), 0);
assert.equal(b.lastIndexOf(buf_a, -b.length), 0);
assert.equal(b.lastIndexOf(buf_a, -b.length - 1), -1);
assert.equal(b.lastIndexOf(buf_a, NaN), 0);
assert.equal(b.lastIndexOf(buf_a, -Infinity), -1);
assert.equal(b.lastIndexOf(buf_a, Infinity), 0);
assert.equal(b.lastIndexOf(buf_bc), 1);
assert.equal(b.lastIndexOf(buf_bc, 2), 1);
assert.equal(b.lastIndexOf(buf_bc, -1), 1);
assert.equal(b.lastIndexOf(buf_bc, -3), 1);
assert.equal(b.lastIndexOf(buf_bc, -5), 1);
assert.equal(b.lastIndexOf(buf_bc, -6), -1);
assert.equal(b.lastIndexOf(buf_bc, NaN), 1);
assert.equal(b.lastIndexOf(buf_bc, -Infinity), -1);
assert.equal(b.lastIndexOf(buf_bc, Infinity), 1);
assert.equal(b.lastIndexOf(buf_f), b.length - 1);
assert.equal(b.lastIndexOf(buf_z), -1);
assert.equal(b.lastIndexOf(buf_empty), -1);
assert.equal(b.lastIndexOf(buf_empty, 1), -1);
assert.equal(b.lastIndexOf(buf_empty, b.length + 1), -1);
assert.equal(b.lastIndexOf(buf_empty, Infinity), -1);
// lastIndexOf number:
assert.equal(b.lastIndexOf(0x61), 0);
assert.equal(b.lastIndexOf(0x61, 1), 0);
assert.equal(b.lastIndexOf(0x61, -1), 0);
assert.equal(b.lastIndexOf(0x61, -4), 0);
assert.equal(b.lastIndexOf(0x61, -b.length), 0);
assert.equal(b.lastIndexOf(0x61, -b.length - 1), -1);
assert.equal(b.lastIndexOf(0x61, NaN), 0);
assert.equal(b.lastIndexOf(0x61, -Infinity), -1);
assert.equal(b.lastIndexOf(0x61, Infinity), 0);
assert.equal(b.lastIndexOf(0x0), -1);

// Test weird offset arguments.
// Behaviour should match String.lastIndexOf:
assert.equal(b.lastIndexOf('b', 0), -1);
assert.equal(b.lastIndexOf('b', undefined), 1);
assert.equal(b.lastIndexOf('b', null), -1);
assert.equal(b.lastIndexOf('b', {}), 1);
assert.equal(b.lastIndexOf('b', []), -1);
assert.equal(b.lastIndexOf('b', [2]), 1);

// Test lastIndexOf on a longer buffer:
var bufferString = new Buffer('a man a plan a canal panama');
assert.equal(15, bufferString.lastIndexOf('canal'));
assert.equal(21, bufferString.lastIndexOf('panama'));
assert.equal(0, bufferString.lastIndexOf('a man a plan a canal panama'));
assert.equal(-1, bufferString.lastIndexOf('a man a plan a canal mexico'));
assert.equal(13, bufferString.lastIndexOf('a '));
assert.equal(13, bufferString.lastIndexOf('a ', 13));
assert.equal(6, bufferString.lastIndexOf('a ', 12));
assert.equal(0, bufferString.lastIndexOf('a ', 5));
assert.equal(13, bufferString.lastIndexOf('a ', -1));
assert.equal(0, bufferString.lastIndexOf('a ', -27));
assert.equal(-1, bufferString.lastIndexOf('a ', -28));

// The above tests test the LINEAR and SINGLE-CHAR strategies.
// Now, we test the BOYER-MOORE-HORSPOOL strategy.
// Test lastIndexOf on a long buffer w multiple matches:
pattern = 'JABACABADABACABA';
assert.equal(1535, longBufferString.lastIndexOf(pattern));
assert.equal(1535, longBufferString.lastIndexOf(pattern, 1535));
assert.equal(511, longBufferString.lastIndexOf(pattern, 1534));

// Finally, give it a really long input to trigger fallback from BMH to
// regular BOYER-MOORE (which has better worst-case complexity).

// Generate a really long Thue-Morse sequence of 'yolo' and 'swag',
// "yolo swag swag yolo swag yolo yolo swag" ..., goes on for about 5MB.
// This is hard to search because it all looks similar, but never repeats.

// countBits returns the number of bits in the binary reprsentation of n.
function countBits(n) {
  for (var count = 0; n > 0; count++) {
    n = n & (n - 1); // remove top bit
  }
  return count;
}
var parts = [];
for (var i = 0; i < 1000000; i++) {
  parts.push((countBits(i) % 2 === 0) ? 'yolo' : 'swag');
}
var reallyLong = new Buffer(parts.join(' '));
assert.equal('yolo swag swag yolo', reallyLong.slice(0, 19).toString());

// Expensive reverse searches. Stress test lastIndexOf:
pattern = reallyLong.slice(0, 100000);  // First 1/50th of the pattern.
assert.equal(4751360, reallyLong.lastIndexOf(pattern));
assert.equal(3932160, reallyLong.lastIndexOf(pattern, 4000000));
assert.equal(2949120, reallyLong.lastIndexOf(pattern, 3000000));
pattern = reallyLong.slice(100000, 200000);  // Second 1/50th.
assert.equal(4728480, reallyLong.lastIndexOf(pattern));
pattern = reallyLong.slice(0, 1000000);  // First 1/5th.
assert.equal(3932160, reallyLong.lastIndexOf(pattern));
pattern = reallyLong.slice(0, 2000000);  // first 2/5ths.
assert.equal(0, reallyLong.lastIndexOf(pattern));
