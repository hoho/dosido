module.exports = function(i, o, sr, params) {
    if (Object.keys(params).length) {
        o.write(JSON.stringify(params) + '\n');
        o.write(JSON.stringify({method: i.method, uri: i.uri, http: i.httpProtocol}) + '\n')
        var input = [];
        i.on('data', function(chunk) { input.push(chunk); });
        i.on('end', function() {
            o.write(input.join(''));
            o.end('\n');
        });
    } else {
        var hello = require('./js2');
        o.write(`${hello()}\n`);
        o.end('world\n');
    }
};
