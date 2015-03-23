# dosido

io.js and nginx melt down.

## Description

dosido is a conjunction of io.js and nginx in one single binary.

In a nginx config you describe a location and tell what JavaScript file to
use for this location. This JavaScript file exports a function. This function
is called to build the response.


## Motivation

Two main reasons for the project:

+ Simplicity.
+ It's just one of three steps of my other project.

### Simplicity

To build a service with io.js (node.js) we need to solve some common problems:

+ Need of something to start an restart io.js (node.js) processes.
+ Need of something to accept connections on port 80 (443) and to pass these
  connections to io.js (node.js) processes.
+ Need of something to serve static files.
+ Need of something for routing.

Starting and restarting might be done with something like upstart scripts,
system.d, or something else.

Distributing the connections might be done with nginx proxy, cluster, or
something else.

Serving static might be done with nginx or other web-server in front.

Routing might be done with some routing framework or just a custom JavaScript
right in your project.

dosido gives the ability to make all the steps in one place:

+ nginx master process is working greatly to start and restart child processes
  and to accept and distribute the connections for years.
+ nginx is the one that usually faces the world to serve static files.
+ nginx config is designed to describe routing, which SSL certificate to use,
  and much more.
+ from the JavaScript handler you can issue nginx subrequests. This means you
  can describe service's input and output (like backends addresses and
  balancing between them) in one config file you already know.


## Example

### nginx.conf (just a relevant fragment)

```
server {
    listen       80;
    server_name  localhost;
    js_root   /usr/local/www/;

    location /test {
        js_param  backend "/my-backend/";
        js_param  addr "$remote_addr";
        js_pass   test.js;
    }

    # Internal location for the backend requests.
    location /my-backend {
        internal;
        rewrite /my-backend(.*) $1 break;
        proxy_set_header Host www.some-amazing-backend.com;
        proxy_pass https://www.some-amazing-backend.com;
    }
}
```

### test.js

```js
module.exports = function(i, o, sr, params) {
    // i: request object.
    // o: response object.
    // sr: a function to make a subrequest.
    // params: an object of js_params, in our case:
    //         {"backend": "/my-backend/", "addr": "127.0.0.1"}.
    var reqBody = [];
    i.on('data', function(chunk) { reqBody.push(chunk); });
    i.on('end', function() {
        // Do a subrequest.
        srResponse = [];
        sr({url: params.backend}, function(r) {
            // r: subrequest object.
            r.on('data', function(chunk) { srResponse.push(chunk); });
            r.on('end', function() {
                var ret = {
                    req: reqBody.join(''),
                    params: params,
                    status: r.statusCode,
                    type: r.getHeader('Last-Modified'),
                    backend: srResponse.join('')
                }
                o.end(JSON.stringify(ret, undefined, 4) +'\n');
            });
        });
    });
}
```

### Response

`curl -d testreq http://localhost/test` will give something like:

```json
{
    "req": "testreq",
    "params": {
        "backend": "/my-backend/",
        "addr": "127.0.0.1"
    },
    "status": 200,
    "type": "Mon, 23 Mar 2015 00:29:48 GMT",
    "backend": "www.some-amazing-backend.com response"
}
```

## How to build

Coming soon.
