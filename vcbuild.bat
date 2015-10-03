@echo off

if /i "%1"=="libnodejs" goto libnodejs
if /i "%1"=="configure" goto configure
if /i "%1"=="nginx" goto nginx

:libnodejs
set GYP_DEFINES="node_target_type=static_library"
nodejs\vcbuild.bat %*
if /i "%1"=="libnodejs" goto exit

:configure
cd nginx
bash auto/configure --with-cc=cl --builddir=objs ^
		--prefix= ^
		--conf-path=conf/nginx.conf ^
		--sbin-path=dosido.exe ^
		--http-log-path=logs/access.log ^
		--error-log-path=logs/error.log ^
		--pid-path=logs/dosido.pid ^
		--lock-path=logs/dosido.lock ^
		--http-client-body-temp-path=temp/client_temp ^
		--http-proxy-temp-path=temp/proxy_temp ^
		--http-fastcgi-temp-path=temp/fastcgi_temp ^
		--http-uwsgi-temp-path=temp/uwsgi_temp ^
		--http-scgi-temp-path=temp/scgi_temp ^
		--add-module=../nginx-nodejs ^
		--with-http_realip_module ^
		--with-http_gzip_static_module ^
		--with-http_secure_link_module ^
		--with-http_stub_status_module ^
		--with-http_ssl_module ^
		--with-select_module ^
		--with-ipv6 ^
		--with-openssl=../nodejs/deps/openssl ^
		--with-pcre=../deps/pcre ^
		--with-zlib=../nodejs/deps/zlib ^
		--with-cc-opt="-I../libnodejs -DFD_SETSIZE=1024 -D_WINSOCK_DEPRECATED_NO_WARNINGS /wd4307"
cd ..
if /i "%1"=="configure" goto exit

:nginx
cd nginx
nmake
cd ..
if /i "%1"=="nginx" goto exit

:exit
