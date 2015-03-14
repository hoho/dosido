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

    //  Codes to come from this script (should match iojsByJSCommandType).
    var BY_JS_INIT_DESTRUCTOR = 1,
        BY_JS_READ_REQUEST_BODY = 2,
        BY_JS_RESPONSE_HEADERS = 3,
        BY_JS_RESPONSE_BODY = 4,
        BY_JS_SUBREQUEST = 5,
        BY_JS_SUBREQUEST_DONE = 6,
    //  Codes to come from nginx (should match iojsToJSCallbackCommandType).
        TO_JS_CALLBACK_CHUNK = 7,
        TO_JS_CALLBACK_SUBREQUEST_HEADERS = 8,
        TO_JS_CALLBACK_REQUEST_ERROR = 9,
        TO_JS_CALLBACK_RESPONSE_ERROR = 10;

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
            var destroy = callback(BY_JS_INIT_DESTRUCTOR, payload, function(what, arg) {
                switch (what) {
                    case TO_JS_CALLBACK_CHUNK:
                        i.push(arg);
                        break;

                    case TO_JS_CALLBACK_REQUEST_ERROR:
                        i.emit('error', arg);
                        break;

                    case TO_JS_CALLBACK_RESPONSE_ERROR:
                        o.emit('error', arg);
                        break;
                }
            });

            o.on('finish', function finish() {
                callback(BY_JS_RESPONSE_BODY, payload, null);
            });

            module(i, o, function subrequest(settings, cb) {
                var sr;

                var url = settings.url || '';
                var method = settings.method || 'GET';
                var headers = settings.headers || {};
                var body = settings.body || null;

                callback(
                    BY_JS_SUBREQUEST,
                    payload,
                    url,                   // SR_URL
                    method,                // SR_METHOD
                    headers,               // SR_HEADERS
                    body,                  // SR_BODY
                    function(what, arg) {  // SR_CALLBACK
                        if (!sr) {
                            sr = new Subrequest((what === TO_JS_CALLBACK_SUBREQUEST_HEADERS) && arg);
                            if (cb) {
                                cb(sr);
                            }
                        }

                        switch (what) {
                            case TO_JS_CALLBACK_REQUEST_ERROR:
                                sr.emit('error', arg);
                                break;

                            case TO_JS_CALLBACK_CHUNK:
                                sr.push(arg);

                                if (arg === null) {
                                    // Notify nginx that subrequest is done.
                                    callback(BY_JS_SUBREQUEST_DONE, payload, null);
                                }

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
                    callback(BY_JS_READ_REQUEST_BODY, payload);
                    requestBodyRequested = true;
                }
            }
        });

        this.resume();
    }


    function Response(callback, payload) {
        var headersSent = false;

        this._headers = {};
        this.statusCode = 200;
        this.statusMessage = 'OK';

        Writable.call(this, {
            write: function write(chunk, encoding, cb) {
                if (!headersSent) {
                    callback(BY_JS_RESPONSE_HEADERS, payload, this._headers);
                    headersSent = true;
                }

                callback(BY_JS_RESPONSE_BODY, payload, chunk.toString());

                cb();
            }
        });
    }

    Response.prototype.setHeader = function setHeader(name, value) {
        this._headers[name] = value;
    };


    function Subrequest(headers) {
        this._headers = headers;

        Readable.call(this, {
            read: function read() {}
        });

        this.resume();
    }

    Subrequest.prototype.getHeader = function getHeader(name) {
        return this._headers[name];
    };
});
