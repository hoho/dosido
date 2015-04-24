var http = require('http');
var fs = require('fs');

/*
{
    "/req1": {
        "expectedRequestHeaders": {
            "X-Request-Test1": "Math.random()",
            "X-Request-Test2": "'Constant text'"
        },
        "expectedRequestBody": "'Hello' + 'world'",

        "responseHeaders": {
            "X-Response-Test1": "Math.random()",
            "X-Response-Test2": "'Constant text'",
            "X-Response-Test3": "REQUEST.getHeader('X-Request-Test2')"
            ...
        },
        "responseBody": "new Array(100).join('ololo');"
    },
    ...
}
*/
var config = JSON.parse(fs.readFileSync(process.argv[2], {encoding: 'utf8'}));

var REQUEST; // Will hold current request info, to be accessible from evals.

var server = http.createServer(function(req, res) {
    var location = config[req.url];
    if (!location) {
        throw new Error('Unknown location `' + req.url + '`');
    }

    var headers = location.responseHeaders;

    var tmp;
    var val;

    REQUEST = req;

    for (tmp in headers) {
        res.setHeader(tmp, eval(headers[tmp]));
    }

    res.writeHead(200, {'Content-Type': 'text/plain'});

    var body = eval(location.responseBody);

    if (body instanceof Array) {
        nextChunk();

        function nextChunk() {
            tmp = body.shift();

            if (body.length) {
                res.write(tmp);
                setTimeout(nextChunk, 100);
            } else {
                res.end(tmp);
            }
        }
    } else {
        res.end(body);
    }
});

server.listen(12346, '127.0.0.1');
