#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotQRBind(BindScalarFunctionInput &input);

//! tp_qr(data, *, ecc, on, off) for VARCHAR and BLOB data
ScalarFunctionSet TextplotQRFunctions();

void TextplotQR(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
