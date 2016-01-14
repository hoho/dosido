'use strict';

require('../common');
const assert = require('assert');
const tls = require('tls');

const singles = 'C=US\nST=CA\nL=SF\nO=Node.js Foundation\nOU=Node.js\nCN=ca1\n'
  + 'emailAddress=ry@clouds.org';
const singlesOut = tls.parseCertString(singles);
assert.deepEqual(singlesOut, {
  C: 'US',
  ST: 'CA',
  L: 'SF',
  O: 'Node.js Foundation',
  OU: 'Node.js',
  CN: 'ca1',
  emailAddress: 'ry@clouds.org'
});

const doubles = 'OU=Domain Control Validated\nOU=PositiveSSL Wildcard\n' +
  'CN=*.nodejs.org';
const doublesOut = tls.parseCertString(doubles);
assert.deepEqual(doublesOut, {
  OU: [ 'Domain Control Validated', 'PositiveSSL Wildcard' ],
  CN: '*.nodejs.org'
});
