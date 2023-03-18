set(ANDROID_ABI "arm64-v8a")
set(ANDROID_NATIVE_API_LEVEL "21")

set(TOOLCHAIN_PREFIX "aarch64-linux-android${ANDROID_NATIVE_API_LEVEL}")

include("$ENV{ANDROID_HOME}/build/cmake/android.toolchain.cmake")
