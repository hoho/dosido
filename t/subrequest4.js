module.exports = function(i, o, sr, params) {
    var status = +i.getHeader('X-Test-Status');
    if (isNaN(status)) {
        var srCount = 4;
        var output = [];

        sr({url: '/srtest4', headers: {'X-Test-Status': '301', 'X-Test-Status-Message': 'Ololo'}}, function(ret) {
            ret.on('data', function(chunk) { output.push(chunk + ' — ' + ret.statusCode + ' (' + ret.statusMessage + ')'); });
            ret.on('end', function() { srDone(); });
        });
        sr({url: '/srtest4', headers: {'X-Test-Status': '400'}}, function(ret) {
            ret.on('data', function(chunk) { output.push(chunk + ' — ' + ret.statusCode + ' (' + ret.statusMessage + ')'); });
            ret.on('end', function() { srDone(); });
        });
        sr({url: '/sr4/srtest4', headers: {'X-Test-Status': '403'}}, function(ret) {
            ret.on('data', function(chunk) { output.push(chunk + ' — ' + ret.statusCode + ' (' + ret.statusMessage + ')'); });
            ret.on('end', function() { srDone(); });
        });
        sr({url: '/sr4/srtest4', headers: {'X-Test-Status': '502'}}, function(ret) {
            ret.on('data', function(chunk) { output.push(chunk + ' — ' + ret.statusCode + ' (' + ret.statusMessage + ')'); });
            ret.on('end', function() { srDone(); });
        });

        function srDone() {
            srCount--;
            if (srCount === 0) {
                output.sort();
                o.end(output.join('|') + '\n');
            }
        }
    } else {
        setTimeout(function() {
            o.statusCode = status;
            o.statusMessage = i.getHeader('X-Test-Status-Message') || '';
            o.end('Status: ' + status);
        }, Math.round(Math.random() * 20));
    }
};
