.PHONY: build
build: libnodejs configure nginx


.PHONY: nodejs
nodejs:
	cd nodejs && GYP_DEFINES="node_target_type=executable" ./configure
	$(MAKE) -C nodejs


.PHONY: libnodejs
libnodejs:
	cd nodejs && ./configure --enable-static
	$(MAKE) -C nodejs


.PHONY: configure
configure:
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
		--add-module=../nginx-nodejs \
		--with-http_realip_module \
		--with-http_gzip_static_module \
		--with-http_secure_link_module \
		--with-http_stub_status_module \
		--with-http_ssl_module \
		--with-ipv6 \
		--with-openssl=../nodejs/deps/openssl \
		--with-pcre=../deps/pcre \
		--with-zlib=../nodejs/deps/zlib \
		--with-cc-opt=-I../libnodejs


.PHONY: nginx
nginx:
	if [ `uname -m` = "x86_64" ]; then export KERNEL_BITS=64; fi && $(MAKE) -C nginx


.PHONY: clean
clean:
	git checkout deps
	$(MAKE) -C nodejs clean
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
	$(MAKE) -C nodejs install
	$(MAKE) -C nginx install


.PHONY: prepare-test
prepare-test: nodejs


.PHONY: test
test:
	# Expecting to have https://github.com/openresty/test-nginx in ../test-nginx.
	# Needs `sudo cpan Test::Nginx`
	./nodejs/out/Release/node t/testrunner.js


# Remove linked nginx binary and make a new one
.PHONY: delete-nginx-binary
delete-nginx-binary:
	rm -f nginx/objs/nginx
.PHONY: relink
relink: libnodejs delete-nginx-binary nginx
