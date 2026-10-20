#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotBarBind(BindScalarFunctionInput &input);

//! tp_bar(value, *, min, max, width, on, off, on_color, off_color, shape, filled, thresholds)
ScalarFunction TextplotBarFunction();

void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
