'use strict';

process.on('SIGQUIT', function() {
    console.info('Got SIGQUIT, exiting.');
    process.exit();
});

process.on('SIGABRT', function() {
    console.info('Got SIGABRT, exiting.');
    process.exit();
});

process.on('SIGTERM', function() {
    console.info('Got SIGTERM, exiting.');
    process.exit();
});

process.on('SIGHUP', function() {
    console.info('Got SIGHUP, exiting.');
    process.exit();
});

process.on('SIGINT', function() {
    console.info('Got SIGINT, exiting.');
    process.exit();
});


(function(require, scripts) {
    var path = require('path');
    var Module = require('module');

    var stream = require('stream');
    var Readable = stream.Readable;
    var Writable = stream.Writable;
    var util = require('util');

    //  Codes to come from this script (should match iojsByJSCommandType).
    var BY_JS_INIT_DESTRUCTOR = 1,
        BY_JS_READ_REQUEST_BODY = 2,
        BY_JS_RESPONSE_HEADERS = 3,
        BY_JS_RESPONSE_BODY = 4,
        BY_JS_BEGIN_SUBREQUEST = 5,
        BY_JS_SUBREQUEST_DONE = 6,
    //  Codes to come from nginx (should match iojsToJSCallbackCommandType).
        TO_JS_CALLBACK_PUSH_CHUNK = 7,
        TO_JS_CALLBACK_SUBREQUEST_HEADERS = 8,
        TO_JS_CALLBACK_REQUEST_ERROR = 9,
        TO_JS_CALLBACK_RESPONSE_ERROR = 10;


    function Request(meta, headers, callback, payload) {
        var requestBodyRequested = false;
        var self = this;

        if (meta) {
            self.method = meta[0];
            self.uri = meta[1];
            self.httpProtocol = meta[2];
        }

        this.headers = headers;

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
    util.inherits(Request, Readable);
    Request.prototype.getHeader = function getHeader(name) {
        return this.headers[name];
    };


    function Response(callback, payload) {
        var headersSent = false;

        this.headers = {};
        this.statusCode = 200;
        this.statusMessage = '';

        Writable.call(this, {
            write: function write(chunk, encoding, cb) {
                if (!headersSent) {
                    var code = +this.statusCode || 200;
                    callback(
                        BY_JS_RESPONSE_HEADERS,
                        payload,
                        [code, this.statusMessage ? code + ' ' + this.statusMessage : ''],
                        this.headers
                    );
                    headersSent = true;
                }

                callback(BY_JS_RESPONSE_BODY, payload, chunk.toString());

                cb();
            }
        });
    }
    util.inherits(Response, Writable);
    Response.prototype.setHeader = function setHeader(name, value) {
        this.headers[name] = value;
    };


    function Subrequest(meta, headers) {
        if (meta) {
            this.statusCode = meta[0];
            this.statusMessage = meta[1];
        }
        this.headers = headers;

        Readable.call(this, {
            read: function read() {}
        });

        this.resume();
    }
    util.inherits(Subrequest, Readable);
    Subrequest.prototype.getHeader = function getHeader(name) {
        return this.headers[name];
    };


    return scripts.map(function(filename) {
        filename = Module._resolveFilename(path.resolve(filename), null);

        var module = new Module(filename, null);

        module.load(filename);
        if (typeof module.exports !== 'function') {
            throw new Error('Module `' + filename + '` should export a function');
        }

        module = module.exports;

        return function(meta, requestHeaders, configParams, callback, payload) {
            var i = new Request(meta, requestHeaders, callback, payload);
            var o = new Response(callback, payload);

            // To free iojsContext when this thing is garbage collected and
            // to pass a from-nginx-to-js callback.
            var destroy = callback(BY_JS_INIT_DESTRUCTOR, payload, function(what, arg) {
                switch (what) {
                    case TO_JS_CALLBACK_PUSH_CHUNK:
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
                    BY_JS_BEGIN_SUBREQUEST,
                    payload,
                    url,                         // SR_URL
                    method,                      // SR_METHOD
                    headers,                     // SR_HEADERS
                    body,                        // SR_BODY
                    function(what, arg, arg2) {  // SR_CALLBACK
                        if (!sr) {
                            if (what === TO_JS_CALLBACK_SUBREQUEST_HEADERS) {
                                sr = new Subrequest(arg, arg2);
                            } else {
                                sr = new Subrequest();
                            }

                            if (cb) {
                                cb(sr);
                            }
                        }

                        switch (what) {
                            case TO_JS_CALLBACK_REQUEST_ERROR:
                                sr.emit('error', arg);
                                break;

                            case TO_JS_CALLBACK_PUSH_CHUNK:
                                sr.push(arg);

                                if (arg === null) {
                                    // Notify nginx that subrequest is done.
                                    callback(BY_JS_SUBREQUEST_DONE, payload, null);
                                }

                                break;
                        }
                    }
                );
            }, configParams);
        };
    });
});
