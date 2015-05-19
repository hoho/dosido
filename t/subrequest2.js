module.exports = function(i, o, sr, params) {
    sr({
        url: '/sr2/ololo?pam=pom',
        method: 'POST',
        headers: {'X-Test': 'Hehe', 'X-Test2': 'Haha'},
        body: new Array(50001).join('bo')
    }, function(ret) {
        o.write(JSON.stringify(ret.headers));
        var data = [];
        ret.on('data', function(chunk) {
            o.write(chunk);
        });
        ret.on('end', function() {
            o.end('\n');
        });
    });
};
