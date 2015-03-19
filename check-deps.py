def extract_version(filename, prefix):
    f = open(filename)
    for line in f.readlines():
        if line.startswith(prefix):
            return line
    raise Exception("Failed to extract version from `%s`" % filename)


iojs_openssl_version = extract_version("./iojs/deps/openssl/openssl/crypto/opensslv.h", "# define OPENSSL_VERSION_NUMBER")
nginx_openssl_version = extract_version("./deps/openssl/crypto/opensslv.h", "# define OPENSSL_VERSION_NUMBER")
if iojs_openssl_version == nginx_openssl_version:
    print "OpenSSL ok."
else:
    print "Error: OpenSSL versions do not match."
    exit(1)


iojs_zlib_version = extract_version("./iojs/deps/zlib/zlib.h", "#define ZLIB_VERSION")
nginx_zlib_version = extract_version("./deps/zlib/zlib.h", "#define ZLIB_VERSION")
if iojs_zlib_version == nginx_zlib_version:
    print "zlib ok."
else:
    print "Error: zlib versions do not match."
    exit(1)
