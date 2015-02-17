.PHONY: all
all: iojs iojsp nginx


.PHONY: iojs
iojs:
	cd iojs && ./configure
	$(MAKE) -C iojs


.PHONY: iojsp
iojsp:
	cd iojs && GYP_DEFINES="iojs_target_type=static_library" ./configure
	$(MAKE) -C iojs


.PHONY: nginx
nginx:
	if [ `uname -m` = "x86_64" ]; then export KERNEL_BITS=64; fi
	cd nginx && ./configure --prefix=/usr/local \
		--conf-path=/etc/dosido/nginx.conf \
		--sbin-path=/usr/local/bin/dosido \
		--http-log-path=/var/log/dosido/access.log \
		--error-log-path=/var/log/dosido/error.log \
		--pid-path=/var/run/dosido.pid \
		--lock-path=/var/run/dosido.lock \
		--http-client-body-temp-path=/var/cache/dosido/client_temp \
		--http-proxy-temp-path=/var/cache/dosido/proxy_temp \
		--http-fastcgi-temp-path=/var/cache/dosido/fastcgi_temp \
		--http-uwsgi-temp-path=/var/cache/dosido/uwsgi_temp \
		--http-scgi-temp-path=/var/cache/dosido/scgi_temp \
		--add-module=../nginx-iojsp \
		--with-http_realip_module \
		--with-http_gzip_static_module \
		--with-http_secure_link_module \
		--with-http_stub_status_module \
		--with-http_ssl_module \
		--with-openssl=../deps/openssl \
		--with-pcre=../deps/pcre \
		--with-zlib=../deps/zlib \
		--with-cc-opt=-I../iojsp
	$(MAKE) -C nginx


.PHONY: clean
clean:
	$(MAKE) -C iojs clean
	rm -rf iojs/out/Release/libiojsp.*
	$(MAKE) -C nginx clean


.PHONY: install
install:
	$(MAKE) -C iojs install
	$(MAKE) -C nginx install
