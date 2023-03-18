set(ANDROID_ABI "armeabi-v7a")
set(ANDROID_NATIVE_API_LEVEL "21")

set(TOOLCHAIN_PREFIX "armv7a-linux-androideabi${ANDROID_NATIVE_API_LEVEL}")

include("$ENV{ANDROID_HOME}/build/cmake/android.toolchain.cmake")
