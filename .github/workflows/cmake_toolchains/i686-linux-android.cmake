set(ANDROID_ABI "x86")
set(ANDROID_NATIVE_API_LEVEL "21")

set(TOOLCHAIN_PREFIX "i686-linux-android${ANDROID_NATIVE_API_LEVEL}")

include("$ENV{ANDROID_HOME}/build/cmake/android.toolchain.cmake")

set(CMAKE_AR "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_STRIP "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-strip" CACHE FILEPATH "strip")
set(CMAKE_OBJCOPY "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-objcopy" CACHE FILEPATH "objcopy")
set(CMAKE_NM "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-nm" CACHE FILEPATH "nm")
