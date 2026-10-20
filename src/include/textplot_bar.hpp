#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotBarBind(BindScalarFunctionInput &input);

void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
