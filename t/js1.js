module.exports = function(i, o, sr, params) {
    o.write('hello\n');
    o.end('world\n');
};
