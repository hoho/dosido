#!/usr/bin/env bash

set -e

rootdir="$(CDPATH= cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
licensefile="${rootdir}/LICENSE"
licensehead="$(sed '/^- /,$d' ${licensefile})"
tmplicense="${rootdir}/~LICENSE.$$"
echo -e "$licensehead" > $tmplicense


# addlicense <library> <location> <license text>
function addlicense {

  echo "
- ${1}, located at ${2}, is licensed as follows:
  \"\"\"
$(echo -e "$3" | sed -e 's/^/    /' -e 's/^    $//' -e 's/ *$//' | sed -e '/./,$!d' | sed -e '/^$/N;/^\n$/D')
  \"\"\"\
" >> $tmplicense

}


if ! [ -f "${rootdir}/deps/icu/license.html" ]; then
  echo "ICU not installed, run configure to download it, e.g. ./configure --with-intl=small-icu --download=icu"
  exit 1
fi


# Dependencies bundled in distributions
addlicense "c-ares" "deps/cares" \
           "$(sed -e '/^ \*\/$/,$d' -e '/^$/d' -e 's/^[/ ]\* *//' ${rootdir}/deps/cares/src/ares_init.c)"
addlicense "HTTP Parser" "deps/http_parser" "$(cat deps/http_parser/LICENSE-MIT)"
addlicense "ICU" "deps/icu" \
           "$(sed -e '1,/ICU License - ICU 1\.8\.1 and later/d' -e :a \
             -e 's/<[^>]*>//g;s/	/ /g;s/ +$//;/</N;//ba' ${rootdir}/deps/icu/license.html)"
addlicense "libuv" "deps/uv" "$(cat ${rootdir}/deps/uv/LICENSE)"
addlicense "OpenSSL" "deps/openssl" \
           "$(sed -e '/^ \*\/$/,$d' -e '/^ [^*].*$/d' -e '/\/\*.*$/d' -e '/^$/d' -e 's/^[/ ]\* *//' ${rootdir}/deps/openssl/openssl/LICENSE)"
addlicense "Punycode.js" "lib/punycode.js" \
           "$(curl -sL https://raw.githubusercontent.com/bestiejs/punycode.js/master/LICENSE-MIT.txt)"
addlicense "V8" "deps/v8" "$(cat ${rootdir}/deps/v8/LICENSE)"
addlicense "zlib" "deps/zlib" \
           "$(sed -e '/The data format used by the zlib library/,$d' -e 's/^\/\* *//' -e 's/^ *//' ${rootdir}/deps/zlib/zlib.h)"

# npm
addlicense "npm" "deps/npm" "$(cat ${rootdir}/deps/npm/LICENSE)"

# Build tools
addlicense "GYP" "tools/gyp" "$(cat ${rootdir}/tools/gyp/LICENSE)"
addlicense "marked" "tools/doc/node_modules/marked" \
           "$(cat ${rootdir}/tools/doc/node_modules/marked/LICENSE)"

# Testing tools
addlicense "cpplint.py" "tools/cpplint.py" \
           "$(sed -e '/^$/,$d' -e 's/^#$//' -e 's/^# //' ${rootdir}/tools/cpplint.py | tail -n +3)"
addlicense "ESLint" "tools/eslint" "$(cat ${rootdir}/tools/eslint/LICENSE)"
addlicense "gtest" "deps/gtest" "$(cat ${rootdir}/deps/gtest/LICENSE)"
addlicense "node-weak" "test/gc/node_modules/weak" \
           "$(cat ${rootdir}/test/gc/node_modules/weak/LICENSE)"


mv $tmplicense $licensefile
