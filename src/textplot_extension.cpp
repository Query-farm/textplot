#include "textplot_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "duckdb/common/vector_operations/senary_executor.hpp"
#include "duckdb/common/vector_operations/septenary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

static const std::unordered_map<std::string, std::vector<std::string>> density_sets = {
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

static unique_ptr<FunctionData> TextplotDensityBind(ClientContext &context, ScalarFunction &bound_function,
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

inline void TextplotDensity(DataChunk &args, ExpressionState &state, Vector &result) {
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

static const std::unordered_map<std::string, std::string> emoji_squares = {
    {"red", "🟥"},    {"orange", "🟧"}, {"yellow", "🟨"}, {"green", "🟩"}, {"blue", "🟦"},
    {"purple", "🟪"}, {"brown", "🟫"},  {"black", "⬛"},  {"white", "⬜"}};

static const std::unordered_map<std::string, std::string> emoji_circles = {
    {"red", "🔴"},    {"orange", "🟠"}, {"yellow", "🟡"}, {"green", "🟢"}, {"blue", "🔵"},
    {"purple", "🟣"}, {"brown", "🟤"},  {"black", "⚫"},  {"white", "⚪"}};

static const std::unordered_map<std::string, std::string> emoji_hearts = {
    {"red", "❤️"},     {"orange", "🧡"}, {"yellow", "💛"}, {"green", "💚"}, {"blue", "💙"},
    {"purple", "💜"}, {"brown", "🤎"},  {"black", "🖤"},  {"white", "🤍"}};

struct TextplotBarBindData : public FunctionData {
	double min = 0;
	double max = 1.0;
	int64_t width = 10;
	string on = "_";
	string off = "*";
	bool filled = true;
	vector<std::pair<double, string>> thresholds;
	string char_shape = "square";
	string on_color = "red";
	string off_color = "white";

	TextplotBarBindData(double min_p, double max_p, int64_t width_p, string on_p, string off_p, bool filled_p,
	                    vector<std::pair<double, string>> thresholds_p, string shape_p, string on_color_p,
	                    string off_color_p)
	    : min(min_p), max(max_p), width(width_p), on(std::move(on_p)), off(std::move(off_p)), filled(filled_p),
	      thresholds(std::move(thresholds_p)), char_shape(std::move(shape_p)), on_color(std::move(on_color_p)),
	      off_color(std::move(off_color_p)) {
	}

private:
	string get_char(const string &color, const string &default_color, const string &shape) const {
		const std::unordered_map<std::string, std::string> *lookup_map = nullptr;
		if (shape == "square") {
			lookup_map = &emoji_squares;
		} else if (shape == "circle") {
			lookup_map = &emoji_circles;
		} else if (shape == "heart") {
			lookup_map = &emoji_hearts;
		} else {
			throw BinderException("tp_bar: 'shape' argument must be one of 'square', 'circle', or 'heart'");
		}

		if (!color.empty()) {
			if (const auto it = lookup_map->find(color); it != lookup_map->end()) {
				return it->second;
			} else {
				throw BinderException(StringUtil::Format("tp_bar: Unknown color value '%s'", color));
			}
		} else {
			return lookup_map->at(default_color);
		}
	}

	std::string get_threshold_color(double n, const string &default_color) const {
		if (thresholds.empty()) {
			return default_color;
		}
		for (const auto &t : thresholds) {
			if (n >= t.first) {
				return t.second;
			}
		}
		return thresholds.back().second;
	}

public:
	string get_character(double value, bool is_on) const {
		// Determine the state of the charaacter.
		// the first state is determine if the user specified the character to use.
		const string off_default = "⬜";

		if (!is_on) {
			if (!off.empty()) {
				return off;
			}
			return get_char(off_color, "white", char_shape);
		} else {
			if (!on.empty()) {
				return on;
			}
			return get_char(get_threshold_color(value, on_color), "red", char_shape);
		}
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<TextplotBarBindData>(min, max, width, on, off, filled, thresholds, char_shape, on_color,
		                                      off_color);
	}
	bool Equals(const FunctionData &other_p) const override {
		return true;
	}
};

static unique_ptr<FunctionData> TextplotBarBind(ClientContext &context, ScalarFunction &bound_function,
                                                vector<unique_ptr<Expression>> &arguments) {
	if (arguments.empty()) {
		throw BinderException("tp_bar takes at least one argument");
	}

	if (!arguments[0]->return_type.IsNumeric()) {
		throw InvalidTypeException("tp_bar first argument must be numeric");
	}

	// Optional arguments

	double min = 0;
	double max = 1.0;
	int64_t width = 10;
	string on = "";
	string off = "";
	string on_color = "";
	string off_color = "";
	string shape = "";
	bool filled = true;
	vector<std::pair<double, string>> thresholds;

	for (idx_t i = 1; i < arguments.size(); i++) {
		const auto &arg = arguments[i];
		if (arg->HasParameter()) {
			throw ParameterNotResolvedException();
		}
		if (!arg->IsFoldable()) {
			throw BinderException("tp_bar: arguments must be constant");
		}
		const auto &alias = arg->GetAlias();
		if (alias == "min") {
			if (!arg->return_type.IsNumeric()) {
				throw BinderException("tp_bar: 'min' argument must be numeric");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			min = eval_result.CastAs(context, LogicalType::DOUBLE).GetValue<double>();
		} else if (alias == "max") {
			if (!arg->return_type.IsNumeric()) {
				throw BinderException("tp_bar: 'max' argument must be numeric");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			max = eval_result.CastAs(context, LogicalType::DOUBLE).GetValue<double>();
		} else if (alias == "thresholds") {
			if (arg->return_type.InternalType() != PhysicalType::LIST) {
				throw BinderException(StringUtil::Format(
				    "tp_bar: 'thresholds' argument must be a list of structs it is %s", arg->return_type.ToString()));
			}

			const auto list_children = ListValue::GetChildren(ExpressionExecutor::EvaluateScalar(context, *arg));
			for (const auto &list_item : list_children) {
				// These should also be lists.
				if (list_item.type().InternalType() != PhysicalType::STRUCT) {
					throw BinderException(
					    StringUtil::Format("tp_bar: 'thresholds' child must be a struct it is %s value is %s",
					                       list_item.type().ToString(), list_item.ToString()));
				}
				// Here you can extract the fields from the struct if needed.
				const auto struct_fields = StructValue::GetChildren(list_item);
				if (struct_fields.size() != 2) {
					throw BinderException(StringUtil::Format(
					    "tp_bar: 'thresholds' child struct must have 2 fields it has %d", struct_fields.size()));
				}
				if (!struct_fields[0].type().IsNumeric()) {
					throw BinderException(StringUtil::Format(
					    "tp_bar: 'thresholds' child struct field 'threshold' must be numeric it is %s",
					    struct_fields[0].type().ToString()));
				}
				const double threshold = struct_fields[0].CastAs(context, LogicalType::DOUBLE).GetValue<double>();
				const string color = struct_fields[1].CastAs(context, LogicalType::VARCHAR).GetValue<string>();
				thresholds.emplace_back(threshold, color);
			}
			std::sort(thresholds.begin(), thresholds.end(), [](const auto &a, const auto &b) {
				return a.first > b.first; // descending by .first
			});
		} else if (alias == "width") {
			if (!arg->return_type.IsIntegral()) {
				throw BinderException("tp_bar: 'width' argument must be an integer");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			width = eval_result.CastAs(context, LogicalType::UBIGINT).GetValue<uint64_t>();
		} else if (alias == "filled") {
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			filled = eval_result.CastAs(context, LogicalType::BOOLEAN).GetValue<bool>();
		} else if (alias == "on") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_bar: 'on' argument must be a VARCHAR");
			}
			on = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "off") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_bar: 'off' argument must be a VARCHAR");
			}
			off = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "off_color") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_bar: 'off_color' argument must be a VARCHAR");
			}
			off_color = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "on_color") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_bar: 'on_color' argument must be a VARCHAR");
			}
			on_color = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "shape") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_bar: 'shape' argument must be a VARCHAR");
			}
			shape = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else {
			throw BinderException(StringUtil::Format("tp_bar: Unknown argument '%s'", alias));
		}
	}

	if (shape.empty()) {
		shape = "square";
	} else {
		if (shape != "square" && shape != "circle" && shape != "heart") {
			throw BinderException("tp_bar: 'shape' argument must be one of 'square', 'circle', or 'heart'");
		}
	}

	return make_uniq<TextplotBarBindData>(min, max, width, on, off, filled, thresholds, shape, on_color, off_color);
}

inline void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &value_vector = args.data[0];
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.bind_info->Cast<TextplotBarBindData>();

	UnaryExecutor::Execute<double, string_t>(value_vector, result, args.size(), [&](double value) {
		const auto proportion = std::clamp((value - bind_data.min) / (bind_data.max - bind_data.min), 0.0, 1.0);
		const auto filled_blocks = static_cast<int64_t>(std::round(bind_data.width * proportion));

		string bar;
		bar.reserve(bind_data.width * 4); // Reserve space for potential multi-byte characters
		for (int64_t i = 0; i < bind_data.width; i++) {
			if (bind_data.filled) {
				// Fill all blocks up to the proportion
				bar += bind_data.get_character(value, i < filled_blocks);
			} else {
				// Only fill the transition point
				bar += bind_data.get_character(value, i == filled_blocks - 1 && filled_blocks > 0);
			}
		}
		return StringVector::AddString(result, bar);
	});
}

static void LoadInternal(DatabaseInstance &instance) {
	auto bar_function = ScalarFunction("tp_bar", {LogicalType::DOUBLE}, LogicalType::VARCHAR, TextplotBar,
	                                   TextplotBarBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, bar_function);

	// Lets register a density function.

	auto density_function =
	    ScalarFunction("tp_density", {LogicalType::LIST(LogicalType::DOUBLE)}, LogicalType::VARCHAR, TextplotDensity,
	                   TextplotDensityBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, density_function);

	// Now lets register some macros to make tp_bar easier to use.
}

void TextplotExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string TextplotExtension::Name() {
	return "textplot";
}

std::string TextplotExtension::Version() const {
#ifdef EXT_VERSION_QUACK
	return EXT_VERSION_QUACK;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void textplot_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::TextplotExtension>();
}

DUCKDB_EXTENSION_API const char *textplot_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}
