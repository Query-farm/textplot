# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(textplot
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
    # Wasm: the loadable-extension emcc link only includes libraries listed
    # here (target_link_libraries is ignored for the SIDE_MODULE link), so the
    # QR-code generator lib must be named explicitly or its symbols are left
    # undefined (loads but throws "n is not a function" on first call).
    LINKED_LIBS "../../vcpkg_installed/wasm32-emscripten/lib/libnayuki-qr-code-generator.a"
)

# Any extra extensions that should be built
# e.g.: duckdb_extension_load(json)