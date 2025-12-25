# =============================================================================
# linux-clang.cmake
# Toolchain for Linux with Clang
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)

# Compiler
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_INIT "-Wall -Wextra -Wpedantic")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0 -fno-omit-frame-pointer")

# Release flags
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

# Use libc++ if available (optional, comment out for libstdc++)
# set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++")

# Position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
