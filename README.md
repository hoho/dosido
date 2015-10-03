# dosido

node.js and nginx melt down.

## Description

dosido is a conjunction of node.js and nginx in one single binary.

In a nginx config you describe a location and tell what JavaScript file to
use for this location. This JavaScript file exports a function. This function
is called to build the response.


## Motivation

Two main reasons for the project:

+ Simplicity.
+ It's just one of three steps of my other project.

### Simplicity

To build a service with node.js we need to solve some common problems:

+ Need of something to start an restart node.js processes.
+ Need of something to accept connections on port 80 (443) and to pass these
  connections to node.js processes.
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
    js_root      /usr/local/www/;

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
module.exports = function(request, response, sr, params) {
    // request:  request object.
    // response: response object.
    // sr:       a function to make a subrequest.
    // params:   an object of js_param, in our case:
    //           {"backend": "/my-backend/", "addr": "127.0.0.1"}.
    var reqBody = [];
    request.on('data', function(chunk) { reqBody.push(chunk); });
    request.on('end', function() {
        // Do a subrequest.
        srData = [];
        sr({url: params.backend}, function(srResponse) {
            // srResponse: subrequest object.
            srResponse.on('data', function(chunk) { srData.push(chunk); });
            srResponse.on('end', function() {
                var ret = {
                    req:      reqBody.join(''),
                    params:   params,
                    status:   srResponse.statusCode,
                    modified: srResponse.getHeader('Last-Modified'),
                    backend:  srData.join('')
                }
                response.end(JSON.stringify(ret, undefined, 4) + '\n');
            });
        });
    });
}
```

### Response

`curl -d testreqbody http://localhost/test` will give something like:

```json
{
    "req": "testreqbody",
    "params": {
        "backend": "/my-backend/",
        "addr": "127.0.0.1"
    },
    "status": 200,
    "modified": "Mon, 23 Mar 2015 00:29:48 GMT",
    "backend": "www.some-amazing-backend.com response"
}
```

## How to build

Coming soon.
