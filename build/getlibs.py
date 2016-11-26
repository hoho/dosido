import sys
import os

build_cmd_file = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                              "out", "Release", ".deps", "out", "Release",
                              "libnodewrap.d")
build_cmd = open(build_cmd_file).read().strip()
libs = build_cmd.split(" ")

if libs[0] != "cmd_out/Release/libnodewrap" and libs[1] != ":=":
    sys.exit(1)

ret = []
skip = False

for l in libs[3:]:
    if l == "-o":
        skip = True
        continue

    if skip:
        skip = False
        continue

    if l.endswith("libnodewrap.o"):
        continue

    if l == "-stdlib=libc++":
        l = "-lc++"

    if l == "-arch":
        skip = True
        continue

    if l == "-mmacosx-version-min=10.7":
        continue

    ret.append(l.replace("out/", "../build/out/"))

print(" ".join(ret))