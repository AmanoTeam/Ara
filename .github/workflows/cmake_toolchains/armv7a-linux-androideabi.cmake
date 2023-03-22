set(ANDROID_ABI "armeabi-v7a")
set(ANDROID_NATIVE_API_LEVEL "21")

set(TOOLCHAIN_PREFIX "armv7a-linux-androideabi${ANDROID_NATIVE_API_LEVEL}")

include("$ENV{ANDROID_HOME}/build/cmake/android.toolchain.cmake")

set(CMAKE_AR "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_STRIP "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-strip" CACHE FILEPATH "strip")
set(CMAKE_OBJCOPY "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-objcopy" CACHE FILEPATH "objcopy")
set(CMAKE_NM "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-nm" CACHE FILEPATH "nm")
