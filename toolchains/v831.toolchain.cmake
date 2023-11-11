# set cross-compiled system type, it's better not use the type which cmake cannot recognized.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
# when hislicon SDK was installed, toolchain was installed in the path as below: 
set(CMAKE_C_COMPILER "/opt/v831-toolchain/bin/arm-openwrt-linux-gcc")
set(CMAKE_CXX_COMPILER "/opt/v831-toolchain/bin/arm-openwrt-linux-g++")
set(CMAKE_FIND_ROOT_PATH "/opt/v831-toolchain")

# set searching rules for cross-compiler
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# set ${CMAKE_C_FLAGS} and ${CMAKE_CXX_FLAGS}flag for cross-compiled process
set(CMAKE_CXX_FLAGS "-march=armv7-a -mfloat-abi=hard -mfpu=neon ${CMAKE_CXX_FLAGS}")

# cache flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "c flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "c++ flags")
