#include "textplot_bar.hpp"
#include "textplot_common.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/common/optional.hpp"
#include <algorithm>
#include <cmath>

namespace duckdb {

// Bar chart emoji lookup tables
const std::unordered_map<std::string, std::string> emoji_squares = {{"red", "🟥"},   {"orange", "🟧"}, {"yellow", "🟨"},
                                                                    {"green", "🟩"}, {"blue", "🟦"},   {"purple", "🟪"},
                                                                    {"brown", "🟫"}, {"black", "⬛"},  {"white", "⬜"}};

const std::unordered_map<std::string, std::string> emoji_circles = {{"red", "🔴"},   {"orange", "🟠"}, {"yellow", "🟡"},
                                                                    {"green", "🟢"}, {"blue", "🔵"},   {"purple", "🟣"},
                                                                    {"brown", "🟤"}, {"black", "⚫"},  {"white", "⚪"}};

const std::unordered_map<std::string, std::string> emoji_hearts = {{"red", "❤️"},    {"orange", "🧡"}, {"yellow", "💛"},
                                                                   {"green", "💚"}, {"blue", "💙"},   {"purple", "💜"},
                                                                   {"brown", "🤎"}, {"black", "🖤"},  {"white", "🤍"}};

// Bar chart bind data structure
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

	string get_character(double value, bool is_on) const {
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
	//! Resolves every colour that get_character can reach, so that an unknown colour is reported
	//! at bind time rather than thrown from inside the execute loop.
	void validate_colors() const {
		if (off.empty()) {
			get_char(off_color, "white", char_shape);
		}
		if (on.empty()) {
			get_char(on_color, "red", char_shape);
			for (const auto &threshold : thresholds) {
				get_char(threshold.second, "red", char_shape);
			}
		}
	}

	unique_ptr<FunctionData> Copy() const override;
	bool Equals(const FunctionData &other_p) const override;

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
			throw BinderException("tp_bar: 'shape' argument must be one of 'square', 'circle', 'heart'");
		}

		if (!color.empty()) {
			if (const auto it = lookup_map->find(color); it != lookup_map->end()) {
				return it->second;
			}
			throw BinderException(StringUtil::Format("tp_bar: Unknown color value '%s'", color));
		}
		return lookup_map->at(default_color);
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
};

unique_ptr<FunctionData> TextplotBarBindData::Copy() const {
	return make_uniq<TextplotBarBindData>(min, max, width, on, off, filled, thresholds, char_shape, on_color,
	                                      off_color);
}

bool TextplotBarBindData::Equals(const FunctionData &other_p) const {
	const auto &other = other_p.Cast<TextplotBarBindData>();
	return min == other.min && max == other.max && width == other.width && on == other.on && off == other.off &&
	       filled == other.filled && thresholds == other.thresholds && char_shape == other.char_shape &&
	       on_color == other.on_color && off_color == other.off_color;
}

unique_ptr<FunctionData> TextplotBarBind(BindScalarFunctionInput &input) {
	auto &context = input.GetClientContext();

	// The options are keyword-only parameters of the signature (see TextplotBarFunction): DuckDB has
	// already matched the names, cast the values to the declared types and filled in the defaults.
	// An explicit NULL option makes the whole call NULL before bind is reached (default NULL handling).
	const auto min = input.GetNonNullConstant("min").GetValue<double>();
	const auto max = input.GetNonNullConstant("max").GetValue<double>();
	const auto width = input.GetNonNullConstant("width").GetValue<int64_t>();
	const auto on = input.GetNonNullConstant("on").GetValue<string>();
	const auto off = input.GetNonNullConstant("off").GetValue<string>();
	const auto on_color = input.GetNonNullConstant("on_color").GetValue<string>();
	const auto off_color = input.GetNonNullConstant("off_color").GetValue<string>();
	auto shape = input.GetNonNullConstant("shape").GetValue<string>();
	const auto filled = input.GetNonNullConstant("filled").GetValue<bool>();

	TextplotValidateCellString("tp_bar", "on", on);
	TextplotValidateCellString("tp_bar", "off", off);

	// 'thresholds' is declared ANY so that any list of two-field structs (whatever the field names)
	// is accepted; its default is an empty list.
	vector<std::pair<double, string>> thresholds;
	const auto thresholds_value = input.GetNonNullConstant("thresholds");
	if (thresholds_value.type().InternalType() != PhysicalType::LIST) {
		throw BinderException(StringUtil::Format("tp_bar: 'thresholds' argument must be a list of structs it is %s",
		                                         thresholds_value.type().ToString()));
	}
	for (const auto &list_item : ListValue::GetChildren(thresholds_value)) {
		if (list_item.type().InternalType() != PhysicalType::STRUCT) {
			throw BinderException(StringUtil::Format("tp_bar: 'thresholds' child must be a struct it is %s value is %s",
			                                         list_item.type().ToString(), list_item.ToString()));
		}
		if (list_item.IsNull()) {
			throw BinderException("tp_bar: 'thresholds' child struct must not be NULL");
		}
		const auto &struct_fields = StructValue::GetChildren(list_item);
		if (struct_fields.size() != 2) {
			throw BinderException(StringUtil::Format("tp_bar: 'thresholds' child struct must have 2 fields it has %d",
			                                         struct_fields.size()));
		}
		if (!struct_fields[0].type().IsNumeric()) {
			throw BinderException(
			    StringUtil::Format("tp_bar: 'thresholds' child struct field 'threshold' must be numeric it is %s",
			                       struct_fields[0].type().ToString()));
		}
		if (struct_fields[0].IsNull() || struct_fields[1].IsNull()) {
			throw BinderException("tp_bar: 'thresholds' child struct fields must not be NULL");
		}
		const double threshold = struct_fields[0].CastAs(context, LogicalType::DOUBLE).GetValue<double>();
		if (!Value::DoubleIsFinite(threshold)) {
			throw BinderException("tp_bar: 'thresholds' child struct field 'threshold' must be finite");
		}
		const string color = struct_fields[1].CastAs(context, LogicalType::VARCHAR).GetValue<string>();
		thresholds.emplace_back(threshold, color);
	}
	std::sort(thresholds.begin(), thresholds.end(), [](const auto &a, const auto &b) {
		return a.first > b.first; // descending by .first
	});

	if (shape.empty()) {
		shape = "square";
	} else if (shape != "square" && shape != "circle" && shape != "heart") {
		throw BinderException("tp_bar: 'shape' argument must be one of 'square', 'circle', 'heart'");
	}

	TextplotValidateWidth("tp_bar", width);

	if (min >= max) {
		throw BinderException("tp_bar: 'min' must be less than 'max'");
	}
	if (!Value::DoubleIsFinite(min) || !Value::DoubleIsFinite(max)) {
		throw BinderException("tp_bar: 'min' and 'max' must be finite");
	}

	// Resolve every colour that can be reached at runtime now, so a typo surfaces as a binder
	// error instead of being thrown from inside the execute loop. Colours are only consulted
	// when the corresponding 'on'/'off' string is empty, so only validate what will be used.
	auto bind_data =
	    make_uniq<TextplotBarBindData>(min, max, width, on, off, filled, thresholds, shape, on_color, off_color);
	bind_data->validate_colors();

	return std::move(bind_data);
}

ScalarFunction TextplotBarFunction() {
	ScalarFunction function("tp_bar", vector<LogicalType> {}, LogicalType::VARCHAR, TextplotBar, TextplotBarBind);
	// tp_bar(value, *, min, max, width, on, off, on_color, off_color, shape, filled, thresholds)
	function.GetSignature()
	    .AddPositionalOnly("value", LogicalType::DOUBLE)
	    .AddKeywordOnly("min", LogicalType::DOUBLE, Value::DOUBLE(0))
	    .AddKeywordOnly("max", LogicalType::DOUBLE, Value::DOUBLE(1))
	    .AddKeywordOnly("width", LogicalType::BIGINT, Value::BIGINT(10))
	    // An empty 'on'/'off' string draws a coloured emoji of the chosen shape instead.
	    .AddKeywordOnly("on", LogicalType::VARCHAR, Value(""))
	    .AddKeywordOnly("off", LogicalType::VARCHAR, Value(""))
	    .AddKeywordOnly("on_color", LogicalType::VARCHAR, Value("red"))
	    .AddKeywordOnly("off_color", LogicalType::VARCHAR, Value("white"))
	    .AddKeywordOnly("shape", LogicalType::VARCHAR, Value("square"))
	    .AddKeywordOnly("filled", LogicalType::BOOLEAN, Value::BOOLEAN(true))
	    .AddKeywordOnly(
	        "thresholds", LogicalType::ANY,
	        Value::LIST(LogicalType::STRUCT({{"threshold", LogicalType::DOUBLE}, {"color", LogicalType::VARCHAR}}),
	                    vector<Value> {}));
	return function;
}

void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &value_vector = args.data[0];
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.BindInfo()->Cast<TextplotBarBindData>();

	UnaryExecutor::Execute<double, string_t>(value_vector, result, [&](double value) -> optional<string_t> {
		if (std::isnan(value)) {
			// NaN has no position on the bar, and rounding it to an integer block count is
			// undefined behaviour; report it as NULL instead.
			return nullopt;
		}

		double proportion;
		if (bind_data.max == bind_data.min) {
			// Avoid division by zero: if value equals min/max, show full bar; otherwise empty
			proportion = (value >= bind_data.min) ? 1.0 : 0.0;
		} else {
			proportion = std::clamp((value - bind_data.min) / (bind_data.max - bind_data.min), 0.0, 1.0);
		}
		// proportion is in [0, 1] and width is capped at bind time, so this cannot overflow.
		const auto filled_blocks = static_cast<int64_t>(std::round(static_cast<double>(bind_data.width) * proportion));

		string bar;
		bar.reserve(static_cast<size_t>(bind_data.width) * 4); // Reserve space for potential multi-byte characters
		for (int64_t i = 0; i < bind_data.width; i++) {
			if (bind_data.filled) {
				// Fill all blocks up to the proportion
				bar += bind_data.get_character(value, i < filled_blocks);
			} else {
				// Only fill the transition point
				bar += bind_data.get_character(value, filled_blocks > 0 && i == filled_blocks - 1);
			}
		}
		return StringVector::AddString(result, bar);
	});
}

} // namespace duckdb
