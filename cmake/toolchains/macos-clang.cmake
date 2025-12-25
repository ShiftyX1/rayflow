# =============================================================================
# macos-clang.cmake
# Toolchain for macOS with AppleClang
# =============================================================================

set(CMAKE_SYSTEM_NAME Darwin)

# Use default system compiler (AppleClang)
# CMake will find it automatically, but we can be explicit:
# set(CMAKE_C_COMPILER /usr/bin/cc)
# set(CMAKE_CXX_COMPILER /usr/bin/c++)

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_INIT "-Wall -Wextra -Wpedantic")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

# Release flags  
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

# macOS deployment target (minimum supported version)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Build for Apple Silicon and Intel (Universal Binary)
# Uncomment for universal builds:
# set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")

# RPATH settings for macOS bundles
set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
