module.exports = function(i, o, sr, params) {
    if (Object.keys(params).length) {
        o.write(JSON.stringify(params) + '\n');
        var input = [];
        i.on('data', function(chunk) { input.push(chunk); });
        i.on('end', function() {
            o.write(input.join(''));
            o.end('\n');
        });
    } else {
        o.write('hello\n');
        o.end('world\n');
    }
};
