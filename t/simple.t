use Test::Nginx::Socket;
 
plan tests => repeat_each() * 2 * blocks();

run_tests();
 
__DATA__
 
=== TEST 1: simple1
--- config
    location /js {
        js_pass ../js1.js;
    }
--- request
GET /js
--- response_body
hello
world
--- error_code: 200



=== TEST 2: simple2
--- config
    js_param p1 "$remote_addr";
    js_param p2 "Hello";
    location /js {
        js_param p2 "World";
        js_param p3 "$query_string";
        js_pass ../js1.js;
    }
--- request
POST /js?arg1=val1&arg2=val2
ololo
piu-piu
--- response_body
{"p2":"World","p3":"arg1=val1&arg2=val2","p1":"127.0.0.1"}
ololo
piu-piu
--- error_code: 200
