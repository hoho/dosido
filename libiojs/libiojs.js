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
    var util = NativeModule.require('util');
    util.inherits(Request, Readable);
    util.inherits(Response, Writable);
    util.inherits(Subrequest, Readable);

    //  Codes to come from this script.
    var INIT_DESTRUCTOR = 1,
        READ_REQUEST_BODY = 2,
        RESPONSE_HEADERS = 3,
        RESPONSE_BODY = 4,
        SUBREQUEST = 5,
    //  Codes to come from nginx.
        CHUNK = 6,
        SUBREQUEST_HEADERS = 7,
        REQUEST_ERROR = 8,
        RESPONSE_ERROR = 9;

    return scripts.map(function(filename) {
        filename = Module._resolveFilename(path.resolve(filename), null);

        var module = new Module(filename, null);

        module.load(filename);
        if (typeof module.exports !== 'function') {
            throw new Error('Module `' + filename + '` should export a function');
        }

        module = module.exports;

        return function(requestHeaders, callback, payload) {
            var i = new Request(requestHeaders, callback, payload);
            var o = new Response(callback, payload);

            // To free iojsContext when this thing is garbage collected and
            // to pass a from-nginx-to-js callback.
            var destroy = callback(INIT_DESTRUCTOR, payload, function(what, arg) {
                switch (what) {
                    case CHUNK:
                        i.push(arg);
                        break;

                    case REQUEST_ERROR:
                        i.emit('error', arg);
                        break;

                    case RESPONSE_ERROR:
                        o.emit('error', arg);
                        break;
                }
            });

            o.on('finish', function finish() {
                callback(RESPONSE_BODY, payload, null);
            });

            module(i, o, function subrequest(settings, cb) {
                var sr;

                var url = settings.url || '';
                var method = settings.method || 'GET';
                var headers = settings.headers || {};
                var body = settings.body || null;

                callback(
                    SUBREQUEST,
                    payload,
                    url,
                    method,
                    headers,
                    body,
                    function(what, arg) {
                        switch (what) {
                            case SUBREQUEST_HEADERS:
                                sr = new Subrequest(arg, callback);
                                cb(undefined, sr);
                                break;

                            case REQUEST_ERROR:
                                cb(arg);
                                break;

                            case CHUNK:
                                if (sr) { sr.push(arg); }
                                break;
                        }
                    }
                );
            });
        };
    });


    function Request(headers, callback, payload) {
        var requestBodyRequested = false;
        var self = this;

        this._headers = headers;

        Readable.call(this, {
            read: function read() {
                if (!requestBodyRequested) {
                    callback(READ_REQUEST_BODY, payload);
                    requestBodyRequested = true;
                }
            }
        });
    }


    function Response(callback, payload) {
        var headersSent = false;

        this._headers = {};
        this.statusCode = 200;
        this.statusMessage = 'OK';

        Writable.call(this, {
            write: function write(chunk, encoding, cb) {
                if (!headersSent) {
                    callback(RESPONSE_HEADERS, payload, this._headers);
                    headersSent = true;
                }

                callback(RESPONSE_BODY, payload, chunk.toString());

                cb();
            }
        });
    }

    Response.prototype.setHeader = function setHeader(name, value) {
        this._headers[name] = value;
    };


    function Subrequest(headers, callback) {
        this._headers = headers;

        Readable.call(this, {
            read: function read() {}
        });
    }

    Subrequest.prototype.getHeader = function getHeader(name) {
        return this._headers[name];
    };
});
