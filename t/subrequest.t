use Test::Nginx::Socket;

plan tests => repeat_each() * 2 * blocks() + 3;

run_tests();

__DATA__

=== TEST 1: subrequest to other server
--- config
    js_root $TEST_NGINX_SERVROOT/..;
    location /srtest1 {
        js_pass subrequest.js;
    }
    location /sr1 {
        internal;
        rewrite /sr1(.*) $1 break;
        proxy_set_header Host dosido.io;
        proxy_set_header X-Piu pau;
        proxy_pass http://127.0.0.1:12346;
    }
--- request
GET /srtest1

--- more_headers
X-Request1: Vavava1
X-Request2: Vavava2

--- response_headers
X-Response1: Value1
X-Response2: Value2
X-Response3:
--- response_body
{"X-Hoho":"Haha","X-Hihi":"Huhu"}|chunk1|chunk2|{"Host":"localhost","Connection":"close","X-Request1":"Vavava1","X-Request2":"Vavava2"}
--- error_code: 200


=== TEST 2: subrequest with long response
--- config
    location /srtest2 {
        js_pass ../subrequest2.js;
    }
    location /sr2 {
        internal;
        rewrite /sr2(.*) $1 break;
        proxy_set_header Host dosido.io;
        proxy_set_header X-Piu pau;
        proxy_pass http://127.0.0.1:12346;
    }
--- request
GET /srtest2
--- response_body eval
"{\"X-Hoho\":\"Haha\",\"X-Hihi\":\"Huhu\"}" . ("a" x 50000) . ("b" x 50000) . "\n"
--- error_code: 200


=== TEST 3: nested subrequests
--- config
    location /srtest3 {
        js_pass ../subrequest3.js;
    }
    location /sr3 {
        internal;
        rewrite /sr3(.*) $1 break;
        proxy_pass http://127.0.0.1:12346;
    }
--- request
GET /srtest3
--- response_body eval
"{\"X-Response1\":\"Response1\"}{\"X-Response4\":\"Response4\"}" . ("a" x 50000) . ("b" x 50000) . ("gh" x 500 x 10) . "\n"
--- error_code: 200
