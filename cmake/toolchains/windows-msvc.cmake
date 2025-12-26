# =============================================================================
# windows-msvc.cmake
# Toolchain for Windows with MSVC (Visual Studio)
# =============================================================================

set(CMAKE_SYSTEM_NAME Windows)

# Force MSVC compiler (avoid MinGW being picked up on CI)
set(CMAKE_C_COMPILER "cl.exe")
set(CMAKE_CXX_COMPILER "cl.exe")

# MSVC is auto-detected, but we set some defaults

# Use static runtime for easier distribution (no VCRUNTIME DLLs needed)
# Comment out if you prefer dynamic runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Compiler flags
set(CMAKE_C_FLAGS_INIT "/W4 /utf-8")
set(CMAKE_CXX_FLAGS_INIT "/W4 /utf-8 /EHsc")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG_INIT "/Od /Zi /RTC1")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Od /Zi /RTC1")

# Release flags
set(CMAKE_C_FLAGS_RELEASE_INIT "/O2 /DNDEBUG /GL")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "/O2 /DNDEBUG /GL")

# Linker flags for whole program optimization in Release
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "/LTCG")

# Suppress some common MSVC warnings that are noisy with external libs
add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
