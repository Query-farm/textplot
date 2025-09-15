#include "textplot_bar.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include <algorithm>

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
};

unique_ptr<FunctionData> TextplotBarBindData::Copy() const {
	return make_uniq<TextplotBarBindData>(min, max, width, on, off, filled, thresholds, char_shape, on_color,
	                                      off_color);
}

bool TextplotBarBindData::Equals(const FunctionData &other_p) const {
	return true;
}

unique_ptr<FunctionData> TextplotBarBind(ClientContext &context, ScalarFunction &bound_function,
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

void TextplotBar(DataChunk &args, ExpressionState &state, Vector &result) {
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

} // namespace duckdb
