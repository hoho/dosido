.PHONY: build
build: libiojs configure nginx


.PHONY: iojs
iojs:
	cd iojs && ./configure
	$(MAKE) -C iojs


.PHONY: libiojs
libiojs:
	cd iojs && ./configure --enable-static
	$(MAKE) -C iojs


.PHONY: check-deps
check-deps:
	@python check-deps.py


.PHONY: configure
configure: check-deps
	rm -rf ./deps/openssl/.openssl
	if [ `uname -m` = "x86_64" ]; then export KERNEL_BITS=64; fi && cd nginx && ./auto/configure \
		--prefix=/usr/local \
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
		--add-module=../nginx-iojs \
		--with-http_realip_module \
		--with-http_gzip_static_module \
		--with-http_secure_link_module \
		--with-http_stub_status_module \
		--with-http_ssl_module \
		--with-ipv6 \
		--with-openssl=../deps/openssl \
		--with-pcre=../deps/pcre \
		--with-zlib=../deps/zlib \
		--with-cc-opt=-I../libiojs


.PHONY: nginx
nginx:
	touch deps/openssl/Makefile
	if [ `uname -m` = "x86_64" ]; then export KERNEL_BITS=64; fi && $(MAKE) -C nginx


.PHONY: clean
clean:
	rm -rf ./deps/openssl/.openssl
	git checkout deps
	$(MAKE) -C iojs clean
	$(MAKE) -C nginx clean


.PHONY: nginx-dirs
nginx-dirs:
	test -d '/var/cache/dosido/client_temp' || mkdir -p '/var/cache/dosido/client_temp'
	test -d '/var/cache/dosido/proxy_temp' || mkdir -p '/var/cache/dosido/proxy_temp'
	test -d '/var/cache/dosido/fastcgi_temp' || mkdir -p '/var/cache/dosido/fastcgi_temp'
	test -d '/var/cache/dosido/uwsgi_temp' || mkdir -p '/var/cache/dosido/uwsgi_temp'
	test -d '/var/cache/dosido/scgi_temp' || mkdir -p '/var/cache/dosido/scgi_temp'
	test -d '/var/log/dosido' || mkdir -p '/var/log/dosido'


.PHONY: install
install: nginx-dirs
	$(MAKE) -C iojs install
	$(MAKE) -C nginx install


.PHONY: prepare-test
prepare-test: iojs


.PHONY: test
test:
	# Expecting to have https://github.com/openresty/test-nginx in ../test-nginx.
	# Needs `sudo cpan Test::Nginx`
	./iojs/iojs t/testrunner.js


# Remove linked nginx binary and make a new one
.PHONY: delete-nginx-binary
delete-nginx-binary:
	rm -f nginx/objs/nginx
.PHONY: relink
relink: libiojs delete-nginx-binary nginx
