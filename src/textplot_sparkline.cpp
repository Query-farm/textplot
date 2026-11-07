#include "textplot_sparkline.hpp"
#include "textplot_common.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/storage/arena_allocator.hpp"
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
std::string generateAbsoluteSparkline(const double *data, int64_t size, int64_t width,
                                      const std::vector<std::string> &characters) {
	if (size == 0 || width == 0 || characters.empty())
		return "";

	double min_val = *std::min_element(data, data + size);
	double max_val = *std::max_element(data, data + size);

	if (max_val == min_val) {
		const auto mid_idx = static_cast<int64_t>(characters.size()) / 2;
		std::string result;
		for (int64_t i = 0; i < width; i++) {
			result += characters[mid_idx];
		}
		return result;
	}

	std::string result;
	double data_per_char = static_cast<double>(size) / static_cast<double>(width);
	const auto max_level = static_cast<int64_t>(characters.size()) - 1;

	for (int64_t i = 0; i < width; i++) {
		int64_t start_idx = static_cast<int64_t>(static_cast<double>(i) * data_per_char);
		int64_t end_idx = static_cast<int64_t>(static_cast<double>(i + 1) * data_per_char);
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

		const auto bucket_size = static_cast<double>(end_idx - start_idx);
		double sum = 0.0;
		for (int64_t j = start_idx; j < end_idx; j++) {
			sum += data[j];
		}
		double avg_val = sum / bucket_size;
		if (!Value::DoubleIsFinite(avg_val)) {
			// The sum of values near the double limit overflowed; average without the intermediate sum.
			avg_val = 0.0;
			for (int64_t j = start_idx; j < end_idx; j++) {
				avg_val += data[j] / bucket_size;
			}
		}

		const double normalized = TextplotFraction(avg_val, min_val, max_val);
		auto level = static_cast<int64_t>(std::round(normalized * static_cast<double>(max_level)));
		level = std::max(static_cast<int64_t>(0), std::min(max_level, level));

		result += characters[level];
	}

	return result;
}

/**
 * Generate sparkline showing directional change (delta mode)
 */
std::string generateDeltaSparkline(const double *data, int64_t size, int64_t width,
                                   const std::vector<std::string> &characters) {
	if (size < 2 || width == 0 || characters.size() < 3)
		return "";

	std::string result;
	// -1 because we're looking at changes
	double data_per_char = static_cast<double>(size - 1) / static_cast<double>(width);

	for (int64_t i = 0; i < width; i++) {
		auto idx = static_cast<int64_t>(static_cast<double>(i) * data_per_char);
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
std::string generateTrendSparkline(const double *data, int64_t size, int64_t width,
                                   const std::vector<std::string> &characters) {
	if (size < 2 || width == 0 || characters.size() < 5)
		return "";

	// Calculate all changes to determine thresholds
	std::vector<double> changes;
	for (int64_t i = 0; i < size - 1; i++) {
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
	const auto change_count = static_cast<int64_t>(changes.size());
	double data_per_char = static_cast<double>(change_count) / static_cast<double>(width);

	for (int64_t i = 0; i < width; i++) {
		auto idx = static_cast<int64_t>(static_cast<double>(i) * data_per_char);
		if (idx >= change_count)
			idx = change_count - 1;

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
std::string generateSparkline(const std::vector<double> &data, int64_t width, const std::string &themeName,
                              SparklineMode mode = SparklineMode::ABSOLUTE) {
	if (data.empty())
		return "";

	auto characters = EnhancedSparklineThemes::getTheme(themeName, mode);
	const auto size = static_cast<int64_t>(data.size());

	switch (mode) {
	case SparklineMode::DELTA:
		return generateDeltaSparkline(data.data(), size, width, characters);
	case SparklineMode::TREND:
		return generateTrendSparkline(data.data(), size, width, characters);
	case SparklineMode::ABSOLUTE:
	default:
		return generateAbsoluteSparkline(data.data(), size, width, characters);
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

//! Reads the keyword-only options shared by tp_sparkline and tp_sparkline_agg.
static unique_ptr<TextplotSparklineBindData> TextplotSparklineBindOptions(const string &function_name,
                                                                          BindFunctionInput &input) {
	// The options are keyword-only parameters of the signature (see TextplotSparklineFunction): DuckDB
	// has already matched the names, cast the values to the declared types and filled in the defaults.
	// An explicit NULL option makes the whole call NULL before bind is reached (default NULL handling).
	const auto width = input.GetNonNullConstant("width").GetValue<int64_t>();
	auto theme = input.GetNonNullConstant("theme").GetValue<string>();
	const auto specified_mode = input.GetNonNullConstant("mode").GetValue<string>();

	SparklineMode mode = SparklineMode::DELTA;
	if (specified_mode == "delta") {
		mode = SparklineMode::DELTA;
	} else if (specified_mode == "trend") {
		mode = SparklineMode::TREND;
	} else if (specified_mode == "absolute") {
		mode = SparklineMode::ABSOLUTE;
	} else {
		throw BinderException(StringUtil::Format("%s: Unknown type '%s' must be one of <delta, trend, absolute>",
		                                         function_name, specified_mode));
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
		throw BinderException(StringUtil::Format("%s: Unknown theme '%s' for mode '%s', available are <%s>",
		                                         function_name, theme, specified_mode,
		                                         StringUtil::Join(available_themes, ", ")));
	}

	TextplotValidateWidth(function_name, width);

	return make_uniq<TextplotSparklineBindData>(mode, theme, width);
}

unique_ptr<FunctionData> TextplotSparklineBind(BindScalarFunctionInput &input) {
	auto &arguments = input.GetArguments();
	if (!TextplotIsNumericList(arguments[0]->GetReturnType())) {
		throw InvalidTypeException("tp_sparkline first argument must be a list of numeric values");
	}
	return TextplotSparklineBindOptions("tp_sparkline", input);
}

void TextplotSparkline(DataChunk &args, ExpressionState &state, Vector &result) {
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.BindInfo()->Cast<TextplotSparklineBindData>();

	TextplotListReader reader(state.GetContext(), args.data[0], args.size());

	std::vector<double> data_items;
	UnaryExecutor::Execute<list_entry_t, string_t>(reader.GetVector(), result, [&](list_entry_t values) {
		reader.Extract(values, data_items);

		// An empty list, or one holding only NULL/NaN/Inf, has nothing to plot.
		if (data_items.empty()) {
			return StringVector::AddString(result, "");
		}

		return StringVector::AddString(result,
		                               generateSparkline(data_items, bind_data.width, bind_data.theme, bind_data.mode));
	});
}

ScalarFunction TextplotSparklineFunction() {
	ScalarFunction function("tp_sparkline", vector<LogicalType> {}, LogicalType::VARCHAR, TextplotSparkline,
	                        TextplotSparklineBind);
	// tp_sparkline(values, *, width, mode, theme)
	function.GetSignature()
	    .AddPositionalOnly("values", LogicalType::LIST(LogicalType::DOUBLE))
	    .AddKeywordOnly("width", LogicalType::BIGINT, Value::BIGINT(20))
	    .AddKeywordOnly("mode", LogicalType::VARCHAR, Value("absolute"))
	    // '' picks the mode's default theme: 'utf8_blocks' for absolute, 'arrows' for delta/trend.
	    .AddKeywordOnly("theme", LogicalType::VARCHAR, Value(""));
	// Casting the input list to LIST(DOUBLE) at execution time can raise an error.
	function.SetFallible();
	return function;
}

// ---------------------------------------------------------------------------------------------
// tp_sparkline_agg: the aggregate form of tp_sparkline
// ---------------------------------------------------------------------------------------------

//! The values of one group, in arrival order. The buffer lives in the aggregate's arena allocator,
//! so the state is trivially movable and needs no destructor.
struct TextplotSparklineAggState {
	double *values;
	idx_t count;
	idx_t capacity;
	//! True once a non-NULL input has been seen (NaN/Inf count as seen but are not stored).
	bool is_set;
};

struct TextplotSparklineAggOperation {
	static bool IgnoreNull() {
		return true;
	}

	static void Append(TextplotSparklineAggState &state, ArenaAllocator &allocator, const double *values, idx_t count) {
		if (count == 0) {
			return;
		}
		if (state.count + count > state.capacity) {
			const auto old_bytes = state.capacity * sizeof(double);
			auto new_capacity = MaxValue<idx_t>(state.capacity * 2, 64);
			while (new_capacity < state.count + count) {
				new_capacity *= 2;
			}
			if (state.values) {
				state.values = reinterpret_cast<double *>(
				    allocator.Reallocate(data_ptr_cast(state.values), old_bytes, new_capacity * sizeof(double)));
			} else {
				state.values = reinterpret_cast<double *>(allocator.Allocate(new_capacity * sizeof(double)));
			}
			state.capacity = new_capacity;
		}
		memcpy(state.values + state.count, values, count * sizeof(double));
		state.count += count;
	}

	template <class INPUT_TYPE, class STATE, class OP>
	static void Operation(STATE &state, const INPUT_TYPE &input, AggregateUnaryInput &unary_input) {
		state.is_set = true;
		const auto value = static_cast<double>(input);
		// NaN and +/-Inf cannot be plotted; drop them the same way tp_sparkline does.
		if (!Value::DoubleIsFinite(value)) {
			return;
		}
		Append(state, unary_input.input.allocator, &value, 1);
	}

	template <class INPUT_TYPE, class STATE, class OP>
	static void ConstantOperation(STATE &state, const INPUT_TYPE &input, AggregateUnaryInput &unary_input,
	                              idx_t count) {
		for (idx_t i = 0; i < count; i++) {
			Operation<INPUT_TYPE, STATE, OP>(state, input, unary_input);
		}
	}

	template <class STATE, class OP>
	static void Combine(const STATE &source, STATE &target, AggregateInputData &aggr_input_data) {
		target.is_set = target.is_set || source.is_set;
		Append(target, aggr_input_data.allocator, source.values, source.count);
	}

	template <class T, class STATE>
	static void Finalize(STATE &state, T &target, AggregateFinalizeData &finalize_data) {
		if (!state.is_set) {
			// No non-NULL input in the group: NULL, like most aggregates.
			finalize_data.ReturnNull();
			return;
		}
		const auto &bind_data = finalize_data.input.bind_data->template Cast<TextplotSparklineBindData>();
		string rendered;
		if (state.count > 0) {
			std::vector<double> data(state.values, state.values + state.count);
			rendered = generateSparkline(data, bind_data.width, bind_data.theme, bind_data.mode);
		}
		target = StringVector::AddString(finalize_data.result, rendered);
	}
};

static unique_ptr<FunctionData> TextplotSparklineAggBind(BindAggregateFunctionInput &input) {
	return TextplotSparklineBindOptions("tp_sparkline_agg", input);
}

AggregateFunction TextplotSparklineAggFunction() {
	using STATE = TextplotSparklineAggState;
	using OP = TextplotSparklineAggOperation;
	AggregateFunction function(
	    "tp_sparkline_agg", vector<LogicalType> {}, LogicalType::VARCHAR, AggregateFunction::StateSize<STATE>,
	    AggregateFunction::StateInitialize<STATE, OP>, AggregateFunction::UnaryScatterUpdate<STATE, double, OP>,
	    AggregateFunction::StateCombine<STATE, OP>, AggregateFunction::StateFinalize<STATE, string_t, OP>,
	    FunctionNullHandling::DEFAULT_NULL_HANDLING, AggregateFunction::NoClusterUpdate(), TextplotSparklineAggBind);
	// tp_sparkline_agg(value, *, width, mode, theme) - same options as tp_sparkline
	function.GetSignature()
	    .AddPositionalOnly("value", LogicalType::DOUBLE)
	    .AddKeywordOnly("width", LogicalType::BIGINT, Value::BIGINT(20))
	    .AddKeywordOnly("mode", LogicalType::VARCHAR, Value("absolute"))
	    .AddKeywordOnly("theme", LogicalType::VARCHAR, Value(""));
	return function;
}

} // namespace duckdb
