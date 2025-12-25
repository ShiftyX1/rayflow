# =============================================================================
# linux-gcc.cmake
# Toolchain for Linux with GCC
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)

# Compiler
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_INIT "-Wall -Wextra -Wpedantic")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

# Release flags
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "")

# Position independent code (for shared libraries if needed)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
