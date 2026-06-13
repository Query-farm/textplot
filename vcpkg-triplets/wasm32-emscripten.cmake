# Overlay of vcpkg's community wasm32-emscripten triplet that lets the wasm
# C/C++ flags be injected per build variant. DuckDB's wasm_eh/wasm_threads
# targets compile the extension with -fwasm-exceptions; vcpkg-built
# dependencies must use the SAME exception ABI or throws cannot cross the
# library boundary (Emscripten JS-EH and wasm-native EH are incompatible).
# The Makefile exports DUCKDB_WASM_VCPKG_CXX_FLAGS per wasm target.

set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSCRIPTEN_ROOT EMSDK PATH)

if(NOT DEFINED ENV{EMSCRIPTEN_ROOT})
   find_path(EMSCRIPTEN_ROOT "emcc")
else()
   set(EMSCRIPTEN_ROOT "$ENV{EMSCRIPTEN_ROOT}")
endif()

if(NOT EMSCRIPTEN_ROOT)
   if(NOT DEFINED ENV{EMSDK})
      message(FATAL_ERROR "The emcc compiler not found in PATH")
   endif()
   set(EMSCRIPTEN_ROOT "$ENV{EMSDK}/upstream/emscripten")
endif()

if(NOT EXISTS "${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
   message(FATAL_ERROR "Emscripten.cmake toolchain file not found")
endif()

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake")

# Match the exception/threading ABI of the DuckDB wasm build variant.
# DUCKDB_WASM_VCPKG_CXX_FLAGS is listed in VCPKG_ENV_PASSTHROUGH (tracked,
# unlike the UNTRACKED list above) so each flag set gets its own vcpkg ABI
# hash and a wasm_eh build can never reuse a wasm_mvp binary-cache entry.
#
# Note: VCPKG_CXX_FLAGS cannot be used here — this triplet chainloads
# Emscripten's own toolchain file, which does not consume vcpkg's flag
# plumbing. Inject the flags directly into each port's configure instead.
set(VCPKG_ENV_PASSTHROUGH DUCKDB_WASM_VCPKG_CXX_FLAGS)
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_C_FLAGS=$ENV{DUCKDB_WASM_VCPKG_CXX_FLAGS} -fPIC"
    "-DCMAKE_CXX_FLAGS=$ENV{DUCKDB_WASM_VCPKG_CXX_FLAGS} -fPIC")
