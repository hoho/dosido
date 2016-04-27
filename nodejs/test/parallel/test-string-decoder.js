'use strict';
require('../common');
var assert = require('assert');
var StringDecoder = require('string_decoder').StringDecoder;

process.stdout.write('scanning ');

// UTF-8
test('utf-8', Buffer.from('$', 'utf-8'), '$');
test('utf-8', Buffer.from('¢', 'utf-8'), '¢');
test('utf-8', Buffer.from('€', 'utf-8'), '€');
test('utf-8', Buffer.from('𤭢', 'utf-8'), '𤭢');
// A mixed ascii and non-ascii string
// Test stolen from deps/v8/test/cctest/test-strings.cc
// U+02E4 -> CB A4
// U+0064 -> 64
// U+12E4 -> E1 8B A4
// U+0030 -> 30
// U+3045 -> E3 81 85
test(
  'utf-8',
  Buffer.from([0xCB, 0xA4, 0x64, 0xE1, 0x8B, 0xA4, 0x30, 0xE3, 0x81, 0x85]),
  '\u02e4\u0064\u12e4\u0030\u3045'
);

// UCS-2
test('ucs2', Buffer.from('ababc', 'ucs2'), 'ababc');

// UTF-16LE
test('ucs2', Buffer.from('3DD84DDC', 'hex'),  '\ud83d\udc4d'); // thumbs up

console.log(' crayon!');

// test verifies that StringDecoder will correctly decode the given input
// buffer with the given encoding to the expected output. It will attempt all
// possible ways to write() the input buffer, see writeSequences(). The
// singleSequence allows for easy debugging of a specific sequence which is
// useful in case of test failures.
function test(encoding, input, expected, singleSequence) {
  var sequences;
  if (!singleSequence) {
    sequences = writeSequences(input.length);
  } else {
    sequences = [singleSequence];
  }
  sequences.forEach(function(sequence) {
    var decoder = new StringDecoder(encoding);
    var output = '';
    sequence.forEach(function(write) {
      output += decoder.write(input.slice(write[0], write[1]));
    });
    process.stdout.write('.');
    if (output !== expected) {
      var message =
        'Expected "' + unicodeEscape(expected) + '", ' +
        'but got "' + unicodeEscape(output) + '"\n' +
        'Write sequence: ' + JSON.stringify(sequence) + '\n' +
        'Decoder charBuffer: 0x' + decoder.charBuffer.toString('hex') + '\n' +
        'Full Decoder State: ' + JSON.stringify(decoder, null, 2);
      assert.fail(output, expected, message);
    }
  });
}

// unicodeEscape prints the str contents as unicode escape codes.
function unicodeEscape(str) {
  var r = '';
  for (var i = 0; i < str.length; i++) {
    r += '\\u' + str.charCodeAt(i).toString(16);
  }
  return r;
}

// writeSequences returns an array of arrays that describes all possible ways a
// buffer of the given length could be split up and passed to sequential write
// calls.
//
// e.G. writeSequences(3) will return: [
//   [ [ 0, 3 ] ],
//   [ [ 0, 2 ], [ 2, 3 ] ],
//   [ [ 0, 1 ], [ 1, 3 ] ],
//   [ [ 0, 1 ], [ 1, 2 ], [ 2, 3 ] ]
// ]
function writeSequences(length, start, sequence) {
  if (start === undefined) {
    start = 0;
    sequence = [];
  } else if (start === length) {
    return [sequence];
  }
  var sequences = [];
  for (var end = length; end > start; end--) {
    var subSequence = sequence.concat([[start, end]]);
    var subSequences = writeSequences(length, end, subSequence, sequences);
    sequences = sequences.concat(subSequences);
  }
  return sequences;
}
