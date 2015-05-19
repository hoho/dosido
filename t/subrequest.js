module.exports = function(i, o, sr, params) {
    o.setHeader('X-Response1', 'Value1');
    sr({
        url: '/sr1/alala',
        method: 'PUT',
        headers: {'X-Test': 'Hehe', 'X-Test2': 'Haha'},
        body: 'Hello body'
    }, function(ret) {
        o.setHeader('X-Response2', 'Value2');
        o.write(JSON.stringify(ret.headers));
        ret.on('data', function(chunk) {
            o.write('|' + chunk);
        });
        ret.on('end', function() {
            o.setHeader('X-Response3', 'Value3');
            o.write('|' + JSON.stringify(i.headers));
            o.end('\n');
        });
    });
};
