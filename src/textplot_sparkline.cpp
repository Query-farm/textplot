#include "textplot_sparkline.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include <algorithm>

namespace duckdb {

/**
 * Sparkline generation modes
 */
enum class SparklineMode {
	ABSOLUTE, // Show absolute values (height-based)
	DELTA,    // Show change direction (up/down/same)
	TREND     // Show trend direction with magnitude
};

/**
 * Enhanced sparkline themes with directional support
 */
class EnhancedSparklineThemes {
public:
	// Absolute value themes (original style)
	static const std::unordered_map<std::string, std::vector<std::string>> absoluteThemes;

	// Delta themes (up/same/down)
	static const std::unordered_map<std::string, std::vector<std::string>> deltaThemes;

	// Trend themes (up/down with magnitude)
	static const std::unordered_map<std::string, std::vector<std::string>> trendThemes;

	static std::vector<std::string> getTheme(const std::string &themeName, SparklineMode mode) {
		const auto *themeMap = &absoluteThemes;

		switch (mode) {
		case SparklineMode::DELTA:
			themeMap = &deltaThemes;
			break;
		case SparklineMode::TREND:
			themeMap = &trendThemes;
			break;
		case SparklineMode::ABSOLUTE:
		default:
			themeMap = &absoluteThemes;
			break;
		}

		auto it = themeMap->find(themeName);
		if (it != themeMap->end()) {
			return it->second;
		}

		// Fallback
		auto fallback = absoluteThemes.find("utf8_blocks");
		return (fallback != absoluteThemes.end()) ? fallback->second : std::vector<std::string> {" ", "█"};
	}

	static vector<std::string> getAvailableThemes(SparklineMode mode) {
		const auto *themeMap = &absoluteThemes;

		switch (mode) {
		case SparklineMode::DELTA:
			themeMap = &deltaThemes;
			break;
		case SparklineMode::TREND:
			themeMap = &trendThemes;
			break;
		case SparklineMode::ABSOLUTE:
		default:
			themeMap = &absoluteThemes;
			break;
		}

		std::vector<std::string> names;
		for (const auto &pair : *themeMap) {
			names.push_back(pair.first);
		}
		std::sort(names.begin(), names.end());
		return names;
	}
};

// Absolute value themes (height-based)
const std::unordered_map<std::string, std::vector<std::string>> EnhancedSparklineThemes::absoluteThemes = {
    {"utf8_blocks", {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"}},
    {"ascii_basic", {" ", ".", "-", "=", "+", "*", "#", "%", "@"}},
    {"hearts", {" ", "🤍", "🤎", "❤️", "💛", "💚", "💙", "💜", "🖤"}},
    {"faces", {" ", "😐", "🙂", "😊", "😃", "😄", "😁", "🤩", "🤯"}}};

// Delta themes (down/same/up) - index 0=down, 1=same, 2=up
const std::unordered_map<std::string, std::vector<std::string>> EnhancedSparklineThemes::deltaThemes = {
    {"arrows", {"↓", "→", "↑"}},       {"triangles", {"▼", "◆", "▲"}},
    {"ascii_arrows", {"v", "-", "^"}}, {"math", {"-", "=", "+"}},
    {"faces", {"😞", "😐", "😊"}},     {"thumbs", {"👎", "👍", "👍"}}, // no neutral thumb, so repeat up
    {"trends", {"📉", "➡️", "📈"}},     {"simple", {"\\", "_", "/"}}};

// Trend themes with magnitude (large down, small down, same, small up, large up)
const std::unordered_map<std::string, std::vector<std::string>> EnhancedSparklineThemes::trendThemes = {
    {"arrows", {"⇩", "↓", "→", "↑", "⇧"}},      {"ascii", {"V", "v", "-", "^", "A"}},
    {"slopes", {"\\\\", "\\", "_", "/", "//"}}, {"intensity", {"--", "-", "=", "+", "++"}},
    {"faces", {"😭", "😞", "😐", "😊", "🤩"}},  {"chart", {"📉", "📊", "➡️", "📊", "📈"}}};

/**
 * Generate sparkline showing absolute values (original behavior)
 */
std::string generateAbsoluteSparkline(const double *data, int size, int width,
                                      const std::vector<std::string> &characters) {
	if (size == 0 || width == 0 || characters.empty())
		return "";

	double min_val = *std::min_element(data, data + size);
	double max_val = *std::max_element(data, data + size);

	if (max_val == min_val) {
		int mid_idx = characters.size() / 2;
		std::string result;
		for (int i = 0; i < width; i++) {
			result += characters[mid_idx];
		}
		return result;
	}

	std::string result;
	double data_per_char = static_cast<double>(size) / width;
	int max_level = static_cast<int>(characters.size()) - 1;

	for (int i = 0; i < width; i++) {
		int start_idx = static_cast<int>(i * data_per_char);
		int end_idx = static_cast<int>((i + 1) * data_per_char);
		// Clamp indices to valid range
		if (start_idx >= size)
			start_idx = size - 1;
		if (end_idx > size)
			end_idx = size;
		if (start_idx >= end_idx)
			end_idx = start_idx + 1;
		// Final safety check: ensure we don't exceed array bounds
		if (end_idx > size)
			end_idx = size;

		double sum = 0.0;
		for (int j = start_idx; j < end_idx; j++) {
			sum += data[j];
		}
		double avg_val = sum / (end_idx - start_idx);

		double normalized = (avg_val - min_val) / (max_val - min_val);
		int level = static_cast<int>(std::round(normalized * max_level));
		level = std::max(0, std::min(max_level, level));

		result += characters[level];
	}

	return result;
}

/**
 * Generate sparkline showing directional change (delta mode)
 */
std::string generateDeltaSparkline(const double *data, int size, int width,
                                   const std::vector<std::string> &characters) {
	if (size < 2 || width == 0 || characters.size() < 3)
		return "";

	std::string result;
	double data_per_char = static_cast<double>(size - 1) / width; // -1 because we're looking at changes

	for (int i = 0; i < width; i++) {
		int idx = static_cast<int>(i * data_per_char);
		if (idx >= size - 1)
			idx = size - 2;

		double current = data[idx];
		double next = data[idx + 1];
		double change = next - current;

		// Determine direction: 0=down, 1=same, 2=up
		int direction = 1; // default to same
		if (change < -1e-10)
			direction = 0; // down
		else if (change > 1e-10)
			direction = 2; // up

		result += characters[direction];
	}

	return result;
}

/**
 * Generate sparkline showing trend with magnitude
 */
std::string generateTrendSparkline(const double *data, int size, int width,
                                   const std::vector<std::string> &characters) {
	if (size < 2 || width == 0 || characters.size() < 5)
		return "";

	// Calculate all changes to determine thresholds
	std::vector<double> changes;
	for (int i = 0; i < size - 1; i++) {
		changes.push_back(data[i + 1] - data[i]);
	}

	// Find thresholds for small vs large changes
	std::vector<double> abs_changes;
	for (double change : changes) {
		if (std::abs(change) > 1e-10) { // ignore near-zero changes
			abs_changes.push_back(std::abs(change));
		}
	}

	double threshold = 0.0;
	if (!abs_changes.empty()) {
		std::sort(abs_changes.begin(), abs_changes.end());
		threshold = abs_changes[abs_changes.size() / 2]; // median
	}

	std::string result;
	double data_per_char = static_cast<double>(changes.size()) / width;

	for (int i = 0; i < width; i++) {
		int idx = static_cast<int>(i * data_per_char);
		if (idx >= static_cast<int>(changes.size()))
			idx = changes.size() - 1;

		double change = changes[idx];
		int level = 2; // default to same (middle)

		if (change < -1e-10) {
			level = (std::abs(change) > threshold) ? 0 : 1; // large down : small down
		} else if (change > 1e-10) {
			level = (std::abs(change) > threshold) ? 4 : 3; // large up : small up
		}

		result += characters[level];
	}

	return result;
}

/**
 * Main sparkline generation function
 */
std::string generateSparkline(const std::vector<double> &data, int width, const std::string &themeName,
                              SparklineMode mode = SparklineMode::ABSOLUTE) {
	if (data.empty())
		return "";

	auto characters = EnhancedSparklineThemes::getTheme(themeName, mode);

	switch (mode) {
	case SparklineMode::DELTA:
		return generateDeltaSparkline(data.data(), data.size(), width, characters);
	case SparklineMode::TREND:
		return generateTrendSparkline(data.data(), data.size(), width, characters);
	case SparklineMode::ABSOLUTE:
	default:
		return generateAbsoluteSparkline(data.data(), data.size(), width, characters);
	}
}

// Bar chart bind data structure
struct TextplotSparklineBindData : public FunctionData {
	SparklineMode mode = SparklineMode::ABSOLUTE;
	string theme = "utf8_blocks";

	int64_t width = 10;

	TextplotSparklineBindData(SparklineMode mode_p, string theme_p, int64_t width_p)
	    : mode(mode_p), theme(std::move(theme_p)), width(width_p) {
	}

	unique_ptr<FunctionData> Copy() const override;
	bool Equals(const FunctionData &other_p) const override;

private:
};

unique_ptr<FunctionData> TextplotSparklineBindData::Copy() const {
	return make_uniq<TextplotSparklineBindData>(mode, theme, width);
}

bool TextplotSparklineBindData::Equals(const FunctionData &other_p) const {
	const auto &other = other_p.Cast<TextplotSparklineBindData>();
	return mode == other.mode && theme == other.theme && width == other.width;
}

unique_ptr<FunctionData> TextplotSparklineBind(ClientContext &context, ScalarFunction &bound_function,
                                               vector<unique_ptr<Expression>> &arguments) {
	if (arguments.empty()) {
		throw BinderException("tp_sparkline takes at least one argument");
	}

	const auto &first_arg = arguments[0]->return_type;
	if (!first_arg.IsNested() || first_arg.InternalType() != PhysicalType::LIST ||
	    !ListType::GetChildType(first_arg).IsNumeric()) {
		throw InvalidTypeException("tp_sparkline first argument must be a list of numeric values");
	}

	// Optional arguments
	int64_t width = 20;
	string theme = "";
	string specified_mode = "absolute";

	for (idx_t i = 1; i < arguments.size(); i++) {
		const auto &arg = arguments[i];
		if (arg->HasParameter()) {
			throw ParameterNotResolvedException();
		}
		if (!arg->IsFoldable()) {
			throw BinderException("tp_sparkline: arguments must be constant");
		}
		const auto &alias = arg->GetAlias();
		if (alias == "width") {
			if (!arg->return_type.IsIntegral()) {
				throw BinderException("tp_sparkline: 'width' argument must be an integer");
			}
			const auto eval_result = ExpressionExecutor::EvaluateScalar(context, *arg);
			width = eval_result.CastAs(context, LogicalType::UBIGINT).GetValue<uint64_t>();
		} else if (alias == "theme") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_sparkline: 'theme' argument must be a VARCHAR");
			}
			theme = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "mode") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_sparkline: 'mode' argument must be a VARCHAR");
			}
			specified_mode = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else {
			throw BinderException(StringUtil::Format("tp_sparkline: Unknown argument '%s'", alias));
		}
	}

	SparklineMode mode = SparklineMode::DELTA;
	if (specified_mode == "delta") {
		mode = SparklineMode::DELTA;
	} else if (specified_mode == "trend") {
		mode = SparklineMode::TREND;
	} else if (specified_mode == "absolute") {
		mode = SparklineMode::ABSOLUTE;
	} else {
		throw BinderException(StringUtil::Format(
		    "tp_sparkline: Unknown type '%s' must be one of <delta, trend, absolute>", specified_mode));
	}

	auto available_themes = EnhancedSparklineThemes::getAvailableThemes(mode);
	if (theme.empty()) {
		// Set default theme based on mode (matching documentation)
		switch (mode) {
		case SparklineMode::ABSOLUTE:
			theme = "utf8_blocks";
			break;
		case SparklineMode::DELTA:
		case SparklineMode::TREND:
			theme = "arrows";
			break;
		}
	}
	if (std::find(available_themes.begin(), available_themes.end(), theme) == available_themes.end()) {
		throw BinderException(StringUtil::Format("tp_sparkline: Unknown theme '%s' for mode '%s', available are <%s>",
		                                         theme, specified_mode, StringUtil::Join(available_themes, ", ")));
	}

	if (width < 1) {
		throw BinderException("tp_sparkline: 'width' argument must be at least 1");
	}

	return make_uniq<TextplotSparklineBindData>(mode, theme, width);
}

void TextplotSparkline(DataChunk &args, ExpressionState &state, Vector &result) {
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.bind_info->Cast<TextplotSparklineBindData>();

	auto &value_vector = args.data[0];
	Vector input_data(LogicalType::LIST(LogicalType::DOUBLE));
	VectorOperations::Cast(state.GetContext(), value_vector, input_data, args.size());

	auto &child_data = ListVector::GetEntry(input_data);
	auto source_data = FlatVector::GetData<double>(child_data);

	UnaryExecutor::Execute<list_entry_t, string_t>(input_data, result, args.size(), [&](list_entry_t values) {
		std::vector<double> data_items;
		data_items.reserve(values.length);

		for (auto i = values.offset; i < values.offset + values.length; i++) {
			data_items.push_back(source_data[i]);
		}

		if (data_items.empty() || bind_data.width <= 0) {
			return StringVector::AddString(result, "");
		}

		return StringVector::AddString(result,
		                               generateSparkline(data_items, bind_data.width, bind_data.theme, bind_data.mode));
	});
}

} // namespace duckdb
