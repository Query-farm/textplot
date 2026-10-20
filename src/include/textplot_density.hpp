#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotDensityBind(BindScalarFunctionInput &input);

void TextplotDensity(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
