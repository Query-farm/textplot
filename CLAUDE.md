# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Textplot is a DuckDB extension that provides text-based data visualization functions. It's a C++ extension built using the DuckDB extension template and CI tools, creating ASCII/Unicode charts directly from SQL queries.

## Build Commands

```bash
# Build release version
VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake GEN=ninja make release

# Build debug version
VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake GEN=ninja make debug

# Run tests (requires build first)
make test           # runs against release build
make test_debug     # runs against debug build
```

The build uses CMake under the hood. Build outputs go to `build/release/` or `build/debug/`.

## Architecture

### Extension Entry Point
- `src/textplot_extension.cpp` - Registers all scalar functions with DuckDB via `LoadInternal()`

### SQL Functions
Each visualization function has a corresponding implementation file:

| Function | Files | Purpose |
|----------|-------|---------|
| `tp_bar()` | `textplot_bar.cpp/.hpp` | Horizontal bar charts with thresholds and colors |
| `tp_density()` | `textplot_density.cpp/.hpp` | Density plots/histograms from arrays |
| `tp_sparkline()` | `textplot_sparkline.cpp/.hpp` | Compact trend lines with multiple modes |
| `tp_qr()` | `textplot_qr.cpp/.hpp` | QR code generation |

### Pattern
Each function follows the DuckDB scalar function pattern:
- `Textplot*Bind()` - Validates and binds arguments at plan time
- `Textplot*()` - Executes the function at runtime
- Functions accept named parameters (e.g., `width := 20`, `style := 'ascii'`)

### Directory Structure
- `src/` - C++ source files
- `src/include/` - Header files
- `test/sql/` - SQLLogicTest files (`.test` extension)
- `duckdb/` - DuckDB submodule (git submodule)
- `extension-ci-tools/` - Build system submodule

## Testing

Tests use DuckDB's SQLLogicTest format in `test/sql/`. See https://duckdb.org/dev/sqllogictest/intro.html for syntax.

## Dependencies

This extension uses vcpkg for dependency management. The `vcpkg.json` in the project root defines dependencies, and `extension_config.cmake` configures the extension build.
