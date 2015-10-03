var spawn = require('child_process').spawn;
var path = require('path');

var testServer = spawn(
    './nodejs/node',
    ['./t/testserver.js', './t/testrunner.json'],
    {stdio: 'inherit', env: process.env}
);

var testRunnerEnv = JSON.parse(JSON.stringify(process.env));
testRunnerEnv.PATH = (testRunnerEnv.PATH ? testRunnerEnv.PATH + ':' : '') + '../dosido/nginx/objs';
testRunnerEnv.TEST_NGINX_SERVROOT = path.join(path.resolve('.'), '..', 'dosido/t/servroot');

var testRunner = spawn(
    'prove',
    ['-r', '../dosido/t'],
    {stdio: 'inherit', env: testRunnerEnv, cwd: '../test-nginx'}
);

testRunner.on('close', function(code) {
    testServer.kill();
});
