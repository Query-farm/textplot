#include "textplot_density.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/extension_util.hpp"
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

// Density plot character sets
extern const std::unordered_map<std::string, std::vector<std::string>> density_sets;

// Density plot bind data structure
struct TextplotDensityBindData : public FunctionData {
	int64_t width = 20;
	std::vector<std::string> density_chars;
	string marker_char;

	TextplotDensityBindData(int64_t width_p, std::vector<std::string> density_chars_p, string marker_char_p)
	    : width(width_p), density_chars(std::move(density_chars_p)), marker_char(std::move(marker_char_p)) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<TextplotDensityBindData>(width, density_chars, marker_char);
	}
	bool Equals(const FunctionData &other_p) const override {
		return true;
	}
};

unique_ptr<FunctionData> TextplotDensityBind(ClientContext &context, ScalarFunction &bound_function,
                                             vector<unique_ptr<Expression>> &arguments) {
	if (arguments.empty()) {
		throw BinderException("tp_density takes at least one argument");
	}

	const auto &first_arg = arguments[0]->return_type;
	if (!first_arg.IsNested() || first_arg.InternalType() != PhysicalType::LIST ||
	    !ListType::GetChildType(first_arg).IsNumeric()) {
		throw InvalidTypeException("tp_density first argument must be a list of numeric values");
	}

	// Optional arguments
	int64_t width = 20;
	std::vector<std::string> graph_characters;
	string marker_char;
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
		} else if (alias == "graph_chars") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_density: 'graph_chars' argument must be a VARCHAR");
			}

			if (arg->return_type.InternalType() != PhysicalType::LIST) {
				throw BinderException(
				    StringUtil::Format("tp_density: 'graph_chars' argument must be a list of strings it is %s",
				                       arg->return_type.ToString()));
			}

			const auto list_children = ListValue::GetChildren(ExpressionExecutor::EvaluateScalar(context, *arg));
			for (const auto &list_item : list_children) {
				// These should also be lists.
				if (list_item.type() != LogicalType::VARCHAR) {
					throw BinderException(
					    StringUtil::Format("tp_density: 'graph_chars' child must be a string it is %s value is %s",
					                       list_item.type().ToString(), list_item.ToString()));
				}
				graph_characters.push_back(StringValue::Get(list_item));
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
	return make_uniq<TextplotDensityBindData>(width, graph_characters, marker_char);
}

void TextplotDensity(DataChunk &args, ExpressionState &state, Vector &result) {
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.bind_info->Cast<TextplotDensityBindData>();

	auto &value_vector = args.data[0];
	Vector input_data(LogicalType::LIST(LogicalType::DOUBLE));
	VectorOperations::Cast(state.GetContext(), value_vector, input_data, args.size());

	auto &child_data = ListVector::GetEntry(input_data);
	auto source_data = FlatVector::GetData<double>(child_data);

	double markerValue = std::nan("");

	UnaryExecutor::Execute<list_entry_t, string_t>(input_data, result, args.size(), [&](list_entry_t values) {
		std::vector<double> data_items;
		data_items.reserve(values.length);

		for (auto i = values.offset; i < values.offset + values.length; i++) {
			data_items.push_back(source_data[i]);
		}

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

			// Add marker if value matches
			if (!std::isnan(markerValue) && std::abs(minVal - markerValue) < 1e-10 && !bind_data.marker_char.empty()) {
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
			if (binIndex >= bind_data.width)
				binIndex = bind_data.width - 1; // Handle edge case
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
		int markerPos = -1;
		if (!std::isnan(markerValue) && markerValue >= minVal && markerValue <= maxVal) {
			markerPos = static_cast<int>((markerValue - minVal) / binWidth);
			if (markerPos >= bind_data.width)
				markerPos = bind_data.width - 1;
		}

		// Generate ASCII representation using provided character set
		std::string output_result;
		const int numLevels = bind_data.density_chars.size() - 1;

		for (int i = 0; i < bind_data.width; i++) {
			// Check if this position should have a marker
			if (i == markerPos && !bind_data.marker_char.empty()) {
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
