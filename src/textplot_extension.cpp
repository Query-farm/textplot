#include "textplot_extension.hpp"
#include "textplot_bar.hpp"
#include "textplot_density.hpp"
#include "textplot_sparkline.hpp"
#include "textplot_qr.hpp"
#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "query_farm_telemetry.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	// tp_bar: Horizontal bar charts with thresholds and colors
	{
		auto bar_function = ScalarFunction("tp_bar", {LogicalType::DOUBLE}, LogicalType::VARCHAR, TextplotBar,
		                                   TextplotBarBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
		CreateScalarFunctionInfo info(std::move(bar_function));

		FunctionDescription desc;
		desc.description = "Creates a horizontal bar chart visualization from a numeric value. "
		                   "Supports customizable width, colors, shapes (square/circle/heart), and color thresholds.";
		desc.parameter_names = {"value", "min", "max", "width", "on", "off", "on_color", "off_color", "shape",
		                        "filled", "thresholds"};
		desc.examples = {"tp_bar(0.75)",
		                 "tp_bar(score, min := 0, max := 100, width := 20)",
		                 "tp_bar(value, on := '#', off := '-', width := 10)",
		                 "tp_bar(pct, shape := 'heart', on_color := 'red')",
		                 "tp_bar(temp, thresholds := [{'threshold': 80, 'color': 'red'}, "
		                 "{'threshold': 50, 'color': 'yellow'}])"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_qr: QR code generation
	{
		auto qr_function = ScalarFunction("tp_qr", {LogicalType::VARCHAR}, LogicalType::VARCHAR, TextplotQR,
		                                  TextplotQRBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
		CreateScalarFunctionInfo info(std::move(qr_function));

		FunctionDescription desc;
		desc.description = "Generates a text-based QR code from a string or blob. "
		                   "Supports configurable error correction levels and custom on/off characters.";
		desc.parameter_names = {"data", "ecc", "on", "off"};
		desc.examples = {"tp_qr('https://duckdb.org')",
		                 "tp_qr(url, ecc := 'high')",
		                 "tp_qr(message, on := '##', off := '  ')"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_density: Density plots/histograms from arrays
	{
		auto density_function =
		    ScalarFunction("tp_density", {LogicalType::LIST(LogicalType::DOUBLE)}, LogicalType::VARCHAR, TextplotDensity,
		                   TextplotDensityBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
		CreateScalarFunctionInfo info(std::move(density_function));

		FunctionDescription desc;
		desc.description = "Creates a density plot (histogram) visualization from an array of numeric values. "
		                   "Supports multiple styles: shaded, dots, ascii, height, circles, safety, rainbow_circle, "
		                   "rainbow_square, moon, sparse, and white.";
		desc.parameter_names = {"values", "width", "style", "marker", "graph_chars"};
		desc.examples = {"tp_density(list(value))",
		                 "tp_density(array_agg(score), width := 40)",
		                 "tp_density(data, style := 'height')",
		                 "tp_density(temps, style := 'rainbow_square', width := 30)"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_sparkline: Compact trend lines with multiple modes
	{
		auto sparkline_function = ScalarFunction("tp_sparkline", {LogicalType::LIST(LogicalType::DOUBLE)},
		                                         LogicalType::VARCHAR, TextplotSparkline, TextplotSparklineBind, nullptr,
		                                         nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
		CreateScalarFunctionInfo info(std::move(sparkline_function));

		FunctionDescription desc;
		desc.description = "Creates a sparkline visualization from an array of numeric values. "
		                   "Supports three modes: 'absolute' (height-based), 'delta' (up/down/same direction), "
		                   "and 'trend' (direction with magnitude). Multiple themes available per mode.";
		desc.parameter_names = {"values", "width", "mode", "theme"};
		desc.examples = {"tp_sparkline(list(value))",
		                 "tp_sparkline(array_agg(price), width := 20)",
		                 "tp_sparkline(data, mode := 'delta', theme := 'arrows')",
		                 "tp_sparkline(temps, mode := 'absolute', theme := 'utf8_blocks')",
		                 "tp_sparkline(stocks, mode := 'trend', theme := 'faces')"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	QueryFarmSendTelemetry(loader, "textplot", TextplotExtension().Version());
}

void TextplotExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}

std::string TextplotExtension::Name() {
	return "textplot";
}

std::string TextplotExtension::Version() const {
	return "2025120401";
}

} // namespace duckdb

extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(textplot, loader) {
	duckdb::LoadInternal(loader);
}
}
