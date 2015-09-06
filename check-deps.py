def extract_version(filename, prefix):
    f = open(filename)
    for line in f.readlines():
        if line.startswith(prefix):
            return line
    raise Exception("Failed to extract version from `%s`" % filename)


iojs_zlib_version = extract_version("./iojs/deps/zlib/zlib.h", "#define ZLIB_VERSION")
nginx_zlib_version = extract_version("./deps/zlib/zlib.h", "#define ZLIB_VERSION")
if iojs_zlib_version == nginx_zlib_version:
    print "zlib ok."
else:
    print "Error: zlib versions do not match."
    exit(1)
