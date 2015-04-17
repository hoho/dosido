use Test::Nginx::Socket;

plan tests => repeat_each() * 2 * blocks() + 2;

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
        proxy_pass http://127.0.0.1:12345;
    }
--- tcp_listen: 12345
--- tcp_no_close
--- tcp_reply eval
["HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nX-Hoho: Haha\r\nX-Hihi: Huhu\r\n\r\n", "6\r\nchunk1\r\n", "6\r\nchunk2\r\n0\r\n\r\n"]
--- request
GET /srtest1
--- response_body
{"X-Hoho":"Haha","X-Hihi":"Huhu"}|chunk1|chunk2
--- tcp_query_len: 109
--- tcp_query eval
"GET /ololo?pam=pom HTTP/1.0\r\nHost: dosido.io\r\nX-Piu: pau\r\nConnection: close\r\nContent-Length: 10\r\n\r\nHello body"
--- error_code: 200
