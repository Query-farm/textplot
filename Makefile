PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=textplot
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# vcpkg dependencies must be compiled with the same exception ABI as the
# DuckDB wasm variant (see vcpkg-triplets/wasm32-emscripten.cmake). JS-EH
# (wasm_mvp) and wasm-native EH (wasm_eh/wasm_threads) cannot be mixed.
wasm_mvp: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fexceptions
wasm_eh: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fwasm-exceptions
wasm_threads: export DUCKDB_WASM_VCPKG_CXX_FLAGS=-fwasm-exceptions -pthread

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile