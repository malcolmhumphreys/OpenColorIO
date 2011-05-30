
INCLUDE(CMakeForceCompiler)

SET(IPHONE_SDK "4.3")
SET(IPHONE_ROOT "/Developer/Platforms/iPhoneSimulator.platform/Developer")
SET(IPHONE_SDK_ROOT "${IPHONE_ROOT}/SDKs/iPhoneSimulator${IPHONE_SDK}.sdk")

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_OSX_ARCHITECTURES i386)
SET(CMAKE_OSX_SYSROOT "${IPHONE_SDK_ROOT}")

CMAKE_FORCE_C_COMPILER("${IPHONE_ROOT}/usr/bin/gcc-4.2" GNU)
CMAKE_FORCE_CXX_COMPILER("${IPHONE_ROOT}/usr/bin/g++-4.2" GNU)

INCLUDE_DIRECTORIES("${IPHONE_SDK_ROOT}/usr/include")
INCLUDE_DIRECTORIES("${IPHONE_SDK_ROOT}/usr/include/c++/4.2.1")
INCLUDE_DIRECTORIES("${IPHONE_SDK_ROOT}/usr/include/c++/4.2.1/armv7-apple-darwin10")

LINK_DIRECTORIES("${IPHONE_SDK_ROOT}/usr/lib")
LINK_DIRECTORIES("${IPHONE_SDK_ROOT}/usr/lib/system")

SET(CMAKE_FIND_ROOT_PATH "${IPHONE_SDK_ROOT}" "${IPHONE_ROOT}")
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(IPHONE_BUILD TRUE)
SET(CMAKE_CROSSCOMPILING TRUE)
