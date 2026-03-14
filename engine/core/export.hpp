#pragma once

// =============================================================================
// export.hpp — DLL export/import macros for engine shared libraries
//
// Each engine module defines RAYFLOW_<MODULE>_EXPORTS when building as a DLL.
// Consumers of the DLL will NOT have this defined, so they get dllimport.
//
// On non-Windows platforms, we use visibility("default") for exported symbols
// (combined with -fvisibility=hidden set at the CMake level).
// =============================================================================

// --- Platform detection ---
#if defined(_WIN32) || defined(__CYGWIN__)
    #define RAYFLOW_DLL_EXPORT __declspec(dllexport)
    #define RAYFLOW_DLL_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
    #define RAYFLOW_DLL_EXPORT __attribute__((visibility("default")))
    #define RAYFLOW_DLL_IMPORT __attribute__((visibility("default")))
#else
    #define RAYFLOW_DLL_EXPORT
    #define RAYFLOW_DLL_IMPORT
#endif

// --- engine_core ---
#ifdef RAYFLOW_CORE_EXPORTS
    #define RAYFLOW_CORE_API RAYFLOW_DLL_EXPORT
#else
    #define RAYFLOW_CORE_API RAYFLOW_DLL_IMPORT
#endif

// --- engine_client ---
#ifdef RAYFLOW_CLIENT_EXPORTS
    #define RAYFLOW_CLIENT_API RAYFLOW_DLL_EXPORT
#else
    #define RAYFLOW_CLIENT_API RAYFLOW_DLL_IMPORT
#endif

// --- engine_ui ---
#ifdef RAYFLOW_UI_EXPORTS
    #define RAYFLOW_UI_API RAYFLOW_DLL_EXPORT
#else
    #define RAYFLOW_UI_API RAYFLOW_DLL_IMPORT
#endif

// --- engine_voxel ---
#ifdef RAYFLOW_VOXEL_EXPORTS
    #define RAYFLOW_VOXEL_API RAYFLOW_DLL_EXPORT
#else
    #define RAYFLOW_VOXEL_API RAYFLOW_DLL_IMPORT
#endif

// --- engine_app (glue: ClientEngine) ---
#ifdef RAYFLOW_APP_EXPORTS
    #define RAYFLOW_APP_API RAYFLOW_DLL_EXPORT
#else
    #define RAYFLOW_APP_API RAYFLOW_DLL_IMPORT
#endif
