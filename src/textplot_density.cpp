#include "textplot_density.hpp"
#include "textplot_common.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/common/types/vector.hpp"
#include <algorithm>
#include <cmath>

namespace duckdb {

// Density plot character sets
const std::unordered_map<std::string, std::vector<std::string>> density_sets = {
    {"shaded", {" ", "░", "▒", "▓", "█"}},
    {"dots", {" ", ".", "•", "●"}},
    {"ascii", {" ", ".", ":", "+", "#", "@"}},
    {"height", {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"}},
    {"circles", {"⚫", "⚪", "🟡", "🟠", "🔴"}},
    {"safety", {"⚫", "🟢", "🟡", "🟠", "🔴", "⚪"}},
    {"rainbow_circle", {"⚫", "🟤", "🟣", "🔵", "🟢", "🟡", "🟠", "🔴", "⚪"}},
    {"rainbow_square", {"⬛", "🟫", "🟪", "🟦", "🟩", "🟨", "🟧", "🟥", "⬜"}},
    {"moon", {"🌑", "🌘", "🌗", "🌖", "🌕"}},
    {"sparse", {" ", "⬜", "▫️", "▪️", "⬛", "⚫"}},
    {"white", {" ", "⚪", "🔘", "⚫"}},
};

// Density plot bind data structure
struct TextplotDensityBindData : public FunctionData {
	int64_t width = 20;
	std::vector<std::string> density_chars;
	string marker_char;
	//! The value to highlight. NaN means no marker was requested.
	double marker_value = std::nan("");

	TextplotDensityBindData(int64_t width_p, std::vector<std::string> density_chars_p, string marker_char_p,
	                        double marker_value_p)
	    : width(width_p), density_chars(std::move(density_chars_p)), marker_char(std::move(marker_char_p)),
	      marker_value(marker_value_p) {
	}

	bool HasMarker() const {
		return !marker_char.empty() && !std::isnan(marker_value);
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<TextplotDensityBindData>(width, density_chars, marker_char, marker_value);
	}
	bool Equals(const FunctionData &other_p) const override {
		const auto &other = other_p.Cast<TextplotDensityBindData>();
		// NaN != NaN, so compare "no marker" explicitly.
		const auto same_marker_value =
		    (std::isnan(marker_value) && std::isnan(other.marker_value)) || marker_value == other.marker_value;
		return width == other.width && density_chars == other.density_chars && marker_char == other.marker_char &&
		       same_marker_value;
	}
};

unique_ptr<FunctionData> TextplotDensityBind(ClientContext &context, ScalarFunction &bound_function,
                                             vector<unique_ptr<Expression>> &arguments) {
	if (arguments.empty()) {
		throw BinderException("tp_density takes at least one argument");
	}

	if (!TextplotIsNumericList(arguments[0]->return_type)) {
		throw InvalidTypeException("tp_density first argument must be a list of numeric values");
	}

	// Optional arguments
	int64_t width = 20;
	std::vector<std::string> graph_characters;
	string marker_char;
	double marker_value = std::nan("");
	string style;

	for (idx_t i = 1; i < arguments.size(); i++) {
		const auto &arg = arguments[i];
		if (arg->HasParameter()) {
			throw ParameterNotResolvedException();
		}
		if (!arg->IsFoldable()) {
			throw BinderException("tp_density: arguments must be constant");
		}
		const auto &alias = arg->GetAlias();
		if (alias == "width") {
			if (!arg->return_type.IsIntegral()) {
				throw BinderException("tp_density: 'width' argument must be an integer");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			width = eval_result.CastAs(context, LogicalType::UBIGINT).GetValue<uint64_t>();
		} else if (alias == "marker") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_density: 'marker' argument must be a VARCHAR");
			}
			marker_char = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
			TextplotValidateCellString("tp_density", "marker", marker_char);
		} else if (alias == "marker_value") {
			if (!arg->return_type.IsNumeric()) {
				throw BinderException("tp_density: 'marker_value' argument must be numeric");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			if (eval_result.IsNull()) {
				throw BinderException("tp_density: 'marker_value' argument must not be NULL");
			}
			marker_value = eval_result.CastAs(context, LogicalType::DOUBLE).GetValue<double>();
			if (!Value::DoubleIsFinite(marker_value)) {
				throw BinderException("tp_density: 'marker_value' argument must be finite");
			}
		} else if (alias == "graph_chars") {
			if (arg->return_type.InternalType() != PhysicalType::LIST) {
				throw BinderException(
				    StringUtil::Format("tp_density: 'graph_chars' argument must be a list of strings it is %s",
				                       arg->return_type.ToString()));
			}

			const auto chars_value = ExpressionExecutor::EvaluateScalar(context, *arg);
			if (chars_value.IsNull()) {
				throw BinderException("tp_density: 'graph_chars' argument must not be NULL");
			}
			const auto list_children = ListValue::GetChildren(chars_value);
			graph_characters.clear();
			for (const auto &list_item : list_children) {
				if (list_item.IsNull()) {
					throw BinderException("tp_density: 'graph_chars' child must not be NULL");
				}
				if (list_item.type() != LogicalType::VARCHAR) {
					throw BinderException(
					    StringUtil::Format("tp_density: 'graph_chars' child must be a string it is %s value is %s",
					                       list_item.type().ToString(), list_item.ToString()));
				}
				const auto character = StringValue::Get(list_item);
				TextplotValidateCellString("tp_density", "graph_chars", character);
				graph_characters.push_back(character);
			}
			if (graph_characters.empty()) {
				throw BinderException("tp_density: 'graph_chars' argument must not be empty");
			}

		} else if (alias == "style") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_density: 'style' argument must be a VARCHAR");
			}
			style = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else {
			throw BinderException(StringUtil::Format("tp_density: Unknown argument '%s'", alias));
		}
	}

	if (!marker_char.empty() && std::isnan(marker_value)) {
		throw BinderException("tp_density: 'marker' requires 'marker_value' to say which value to highlight");
	}
	if (marker_char.empty() && !std::isnan(marker_value)) {
		// The value alone is enough to ask for a marker; pick a character that reads as a pointer.
		marker_char = "▼";
	}

	if (!graph_characters.empty() && !style.empty()) {
		throw BinderException("tp_density: 'graph_chars' and 'style' arguments are mutually exclusive");
	}
	if (graph_characters.empty() && style.empty()) {
		style = "shaded";
	}

	if (!style.empty()) {
		// lookup the style in density_sets
		if (const auto it = density_sets.find(style); it != density_sets.end()) {
			graph_characters = it->second;
		} else {
			throw BinderException(StringUtil::Format("tp_density: Unknown style '%s'", style));
		}
	}

	TextplotValidateWidth("tp_density", width);

	return make_uniq<TextplotDensityBindData>(width, graph_characters, marker_char, marker_value);
}

void TextplotDensity(DataChunk &args, ExpressionState &state, Vector &result) {
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.bind_info->Cast<TextplotDensityBindData>();

	TextplotListReader reader(state.GetContext(), args.data[0], args.size());

	std::vector<double> data_items;
	UnaryExecutor::Execute<list_entry_t, string_t>(reader.GetVector(), result, args.size(), [&](list_entry_t values) {
		reader.Extract(values, data_items);

		// An empty list, or one holding only NULL/NaN/Inf, has nothing to plot.
		if (data_items.empty() || bind_data.width <= 0 || bind_data.density_chars.empty()) {
			return StringVector::AddString(result, "");
		}

		// Find min and max values
		const auto minmax = std::minmax_element(data_items.cbegin(), data_items.cend());
		const double minVal = *minmax.first;
		const double maxVal = *minmax.second;

		if (minVal == maxVal) {
			// All values are the same - use max density character
			const auto &maxChar = bind_data.density_chars.back();
			std::vector<string> output_items(bind_data.width, maxChar);

			// Every bin holds the same value, so the marker either covers the whole plot or none of it.
			if (bind_data.HasMarker() && std::abs(minVal - bind_data.marker_value) < 1e-10) {
				std::fill(output_items.begin(), output_items.end(), bind_data.marker_char);
			}

			// Now join all of the items without commas
			std::string output_result;
			for (const auto &item : output_items) {
				output_result += item;
			}
			return StringVector::AddString(result, output_result);
		}

		// Create histogram bins
		std::vector<int> bins(bind_data.width, 0);
		const double range = maxVal - minVal;
		const double binWidth = range / bind_data.width;

		// Count values in each bin
		for (const double val : data_items) {
			auto binIndex = static_cast<int>((val - minVal) / binWidth);
			// Clamp to valid range to handle floating point edge cases
			if (binIndex < 0)
				binIndex = 0;
			if (binIndex >= bind_data.width)
				binIndex = bind_data.width - 1;
			bins[binIndex]++;
		}

		// Find max count for scaling
		const int maxCount = *std::max_element(bins.cbegin(), bins.cend());
		if (maxCount == 0) {
			const auto &maxChar = bind_data.density_chars.front();
			std::string output_result;
			for (int64_t i = 0; i < bind_data.width; i++) {
				output_result += maxChar;
			}

			return StringVector::AddString(result, output_result);
		}

		// Determine marker position if specified
		int64_t markerPos = -1;
		if (bind_data.HasMarker() && bind_data.marker_value >= minVal && bind_data.marker_value <= maxVal) {
			markerPos = static_cast<int64_t>((bind_data.marker_value - minVal) / binWidth);
			// Clamp to valid range to handle floating point edge cases
			if (markerPos < 0)
				markerPos = 0;
			if (markerPos >= bind_data.width)
				markerPos = bind_data.width - 1;
		}

		// Generate ASCII representation using provided character set
		std::string output_result;
		const int numLevels = bind_data.density_chars.size() - 1;

		for (int64_t i = 0; i < bind_data.width; i++) {
			// Check if this position should have a marker
			if (i == markerPos) {
				output_result += bind_data.marker_char;
			} else {
				// Scale bin count to character range
				const auto normalized = static_cast<double>(bins[i]) / maxCount;
				auto charIndex = static_cast<int>(normalized * numLevels + 0.5);
				charIndex = std::min(charIndex, numLevels);
				output_result += bind_data.density_chars[charIndex];
			}
		}

		return StringVector::AddString(result, output_result);
	});
}

} // namespace duckdb
