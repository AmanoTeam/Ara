set(ANDROID_ABI "x86")
set(ANDROID_NATIVE_API_LEVEL "21")

set(TOOLCHAIN_PREFIX "i686-linux-android${ANDROID_NATIVE_API_LEVEL}")

include("$ENV{ANDROID_HOME}/build/cmake/android.toolchain.cmake")