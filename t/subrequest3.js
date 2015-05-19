var assertDeepEqual = require('assert').deepEqual;

module.exports = function(i, o, sr, params) {
    sr({
        url: '/sr3/1',
        method: 'POST',
        headers: {'X-Test1': 'Value1'},
        body: new Array(50001).join('sr1')
    }, function(ret) {
        o.write(JSON.stringify(ret.headers));
        var data1 = [];
        ret.on('data', function(chunk) { data1.push(chunk); });
        ret.on('end', function() {
            sr({
                url: '/sr3/4',
                method: 'PUT',
                headers: {'X-Test4': 'Value4'},
                body: new Array(50001).join('sr4')
            }, function(ret) {
                o.write(JSON.stringify(ret.headers));
                o.setHeader('X-Ololo', 'Piu');
                o.write(data1.join(''));
                var data4 = [];
                ret.on('data', function(chunk) { data4.push(chunk); });
                ret.on('end', function() {
                    assertDeepEqual(data4.join(''), new Array(11).join(new Array(501).join('gh')));

                    var remain = 2;

                    sr({
                        url: '/sr3/5',
                        method: 'PUT',
                        headers: {'X-Test5': 'Value5'},
                        body: new Array(50001).join('sr5')
                    }, function(ret) {
                        var data5 = [];
                        ret.on('data', function(chunk) { data5.push(chunk); });
                        ret.on('end', function() {
                            assertDeepEqual(data5.join(''), new Array(50001).join('i') + new Array(50001).join('j'));
                            if (--remain === 0) {
                                o.end('\n');
                            }
                        })
                    });

                    sr({
                        url: '/sr3/6',
                        method: 'PUT',
                        headers: {'X-Test6': 'Value6'},
                        body: new Array(50001).join('sr6')
                    }, function(ret) {
                        var data6 = [];
                        ret.on('data', function(chunk) { data6.push(chunk); });
                        ret.on('end', function() {
                            assertDeepEqual(data6.join(''), new Array(50001).join('k') + new Array(50001).join('l'));
                            if (--remain === 0) {
                                o.end('\n');
                            }
                        })
                    });

                    o.write(data4.join(''));
                })
            });
        });

        sr({
            url: '/sr3/3',
            method: 'PUT',
            headers: {'X-Test3': 'Value3'}
        }, function(ret) {
            var data3 = [];
            ret.on('data', function(chunk) { data3.push(chunk); });
            ret.on('end', function() {
                assertDeepEqual(data3.join(''), new Array(50001).join('e') + new Array(50001).join('f'));
            });
        });
    });

    sr({
        url: '/sr3/2',
        method: 'PUT',
        headers: {'X-Test2': 'Value2'},
        body: new Array(50001).join('sr2')
    }, function(ret) {
        var data2 = [];
        ret.on('data', function(chunk) { data2.push(chunk); });
        ret.on('end', function() {
            assertDeepEqual(data2.join(''), new Array(50001).join('c') + new Array(50001).join('d'));
        });
    });
};
