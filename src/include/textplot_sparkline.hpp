#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/common/exception.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Function declarations
unique_ptr<FunctionData> TextplotSparklineBind(BindScalarFunctionInput &input);

//! tp_sparkline(values, *, width, mode, theme)
ScalarFunction TextplotSparklineFunction();

void TextplotSparkline(DataChunk &args, ExpressionState &state, Vector &result);

//! tp_sparkline_agg(value, *, width, mode, theme): aggregate form, values plotted in input order
//! (use ORDER BY inside the call to fix that order).
AggregateFunction TextplotSparklineAggFunction();

} // namespace duckdb
