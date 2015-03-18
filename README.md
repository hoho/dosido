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
