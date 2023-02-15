set(CMAKE_SYSTEM_NAME iOS)
set(TOOLCHAIN_PREFIX armv7-apple-darwin)

set(TOOLCHAIN_PATH $ENV{THEOS_HOME})

set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PATH}/sdks/iPhoneOS11.4.sdk)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/toolchain/linux/iphone/bin/clang -target ${TOOLCHAIN_PREFIX} -isysroot ${CMAKE_FIND_ROOT_PATH})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/toolchain/linux/iphone/bin/clang++ -target ${TOOLCHAIN_PREFIX} -isysroot ${CMAKE_FIND_ROOT_PATH})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_compile_definitions(__AppleiOS__)
add_compile_options(-fno-optimize-sibling-calls)
