@echo off
﻿set GYP_DEFINES="iojs_target_type=static_library"
iojs\vcbuild.bat %*
