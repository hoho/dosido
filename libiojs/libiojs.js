'use strict';

(function(scripts) {
    // The following is copied from node.js.
    var ContextifyScript = process.binding('contextify').ContextifyScript;
    function runInThisContext(code, options) {
        var script = new ContextifyScript(code, options);
        return script.runInThisContext();
    }

    function NativeModule(id) {
        this.filename = id + '.js';
        this.id = id;
        this.exports = {};
        this.loaded = false;
    }

    NativeModule._source = process.binding('natives');
    NativeModule._cache = {};

    NativeModule.require = function(id) {
        if (id == 'native_module') {
            return NativeModule;
        }

        var cached = NativeModule.getCached(id);
        if (cached) {
            return cached.exports;
        }

        if (!NativeModule.exists(id)) {
            throw new Error('No such native module ' + id);
        }

        process.moduleLoadList.push('NativeModule ' + id);

        var nativeModule = new NativeModule(id);

        nativeModule.cache();
        nativeModule.compile();

        return nativeModule.exports;
    };

    NativeModule.getCached = function(id) {
        return NativeModule._cache[id];
    };

    NativeModule.exists = function(id) {
        return NativeModule._source.hasOwnProperty(id);
    };

    NativeModule.getSource = function(id) {
        return NativeModule._source[id];
    };

    NativeModule.wrap = function(script) {
        return NativeModule.wrapper[0] + script + NativeModule.wrapper[1];
    };

    NativeModule.wrapper = [
        '(function (exports, require, module, __filename, __dirname) { ',
        '\n});'
    ];

    NativeModule.prototype.compile = function() {
        var source = NativeModule.getSource(this.id);
        source = NativeModule.wrap(source);

        var fn = runInThisContext(source, { filename: this.filename });
        fn(this.exports, NativeModule.require, this, this.filename);

        this.loaded = true;
    };

    NativeModule.prototype.cache = function() {
        NativeModule._cache[this.id] = this;
    };


    // Actually load scripts.
    var path = NativeModule.require('path');
    var Module = NativeModule.require('module');

    var stream = NativeModule.require('stream');
    var Readable = stream.Readable;
    var Writable = stream.Writable;
    var Duplex = stream.Duplex;
    var util = NativeModule.require('util');
    util.inherits(Request, Readable);
    util.inherits(Response, Writable);
    util.inherits(Subrequest, Duplex);

    return scripts.map(function(filename) {
        filename = Module._resolveFilename(path.resolve(filename), null);

        var module = new Module(filename, null);

        module.load(filename);
        if (typeof module.exports !== 'function') {
            throw new Error('Module `' + filename + '` should export a function');
        }

        module = module.exports;

        var INIT_DESTRUCTOR = 0,
            READ_REQUEST_BODY = 1,
            RESPONSE_HEADERS = 2,
            RESPONSE_BODY = 3,
            SUBREQUEST = 4;

        return function(requestHeaders, callback, payload) {
            // To free iojsContext when this thing is garbage collected.
            var destroy = callback(INIT_DESTRUCTOR, payload);

            var requestBodyRequested = false;
            var headersSent = false;

            var i = new Request(requestHeaders, function read() {
                if (!requestBodyRequested) {
                    callback(READ_REQUEST_BODY, payload, function(chunk) {
                        i.push(chunk);
                    });
                    requestBodyRequested = true;
                }
            });

            var o = new Response(function write(chunk, encoding, cb) {
                if (!headersSent) {
                    callback(RESPONSE_HEADERS, payload, this._headers);
                    headersSent = true;
                }

                callback(RESPONSE_BODY, payload, chunk.toString());

                cb();
            });

            o.on('finish', function finish() {
                callback(RESPONSE_BODY, payload, null);
            });

            module(i, o, function subrequest() {
                return new Subrequest(
                    function read() {

                    },
                    function write() {

                    }
                );
            });
        };
    });


    function Request(headers, read) {
        this._headers = headers;
        Readable.call(this, {read: read});
    }


    function Response(write) {
        this._headers = {};
        this.statusCode = 200;
        this.statusMessage = 'OK';
        Writable.call(this, {write: write});
    }

    Response.prototype.setHeader = function setHeader(name, value) {
        this._headers[name] = value;
    };


    function Subrequest(read, write) {
        Duplex.call(this, {
            read: read,
            write: write
        })
    }
});
