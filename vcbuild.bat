@echo off
set GYP_DEFINES="iojs_target_type=static_library"
iojs\vcbuild.bat %*

cd nginx
bash auto/configure --with-cc=cl --builddir=objs \
		--prefix= \
		--conf-path=conf/nginx.conf \
		--sbin-path=dosido.exe \
		--http-log-path=logs/access.log \
		--error-log-path=logs/error.log \
		--pid-path=logs/dosido.pid \
		--lock-path=logs/dosido.lock \
		--http-client-body-temp-path=temp/client_temp \
		--http-proxy-temp-path=temp/proxy_temp \
		--http-fastcgi-temp-path=temp/fastcgi_temp \
		--http-uwsgi-temp-path=temp/uwsgi_temp \
		--http-scgi-temp-path=temp/scgi_temp \
		--add-module=../nginx-iojs \
		--with-http_realip_module \
		--with-http_gzip_static_module \
		--with-http_secure_link_module \
		--with-http_stub_status_module \
		--with-http_ssl_module \
		--with-select_module \
		--with-ipv6 \
		--with-openssl=../deps/openssl \
		--with-pcre=../deps/pcre \
		--with-zlib=../deps/zlib \
		--with-cc-opt="-I../libiojs -DFD_SETSIZE=1024 -D_WINSOCK_DEPRECATED_NO_WARNINGS"
