#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotBarBind(ClientContext &context, ScalarFunction &bound_function,
                                         vector<unique_ptr<Expression>> &arguments);

void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
