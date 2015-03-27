use Test::Nginx::Socket;
 
repeat_each(2);
plan tests => repeat_each() * 2;

no_shuffle();
run_tests();
 
__DATA__
 
=== TEST 1: simple
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
