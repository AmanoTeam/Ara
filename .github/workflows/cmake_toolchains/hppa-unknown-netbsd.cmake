set(CMAKE_SYSTEM_NAME "NetBSD")
set(CMAKE_SYSTEM_PROCESSOR "hppa")

set(TOOLCHAIN_PREFIX "${CMAKE_SYSTEM_PROCESSOR}-unknown-netbsd")

set(TOOLCHAIN_PATH "$ENV{NETBSD_TOOLCHAIN}")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-g++")
set(CMAKE_AR "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-ar" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_STRIP "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-strip" CACHE FILEPATH "strip")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-objcopy" CACHE FILEPATH "objcopy")

set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
