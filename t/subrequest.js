module.exports = function(i, o, sr, params) {
    sr({
        url: '/sr1/ololo?pam=pom',
        method: 'PUT',
        headers: {'X-Test': 'Hehe', 'X-Test2': 'Haha'},
        body: 'Hello body'
    }, function(ret) {
        o.write(JSON.stringify(ret._headers));
        ret.on('data', function(chunk) {
            o.write('|' + chunk);
        });
        ret.on('end', function() {
            o.end('\n');
        });
    });
};
