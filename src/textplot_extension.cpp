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
	// Each function declares its options as keyword-only parameters with defaults, so DuckDB matches
	// the names, casts the values and fills in defaults; duckdb_functions() reports the parameter
	// names and types straight from those signatures.

	// tp_bar: Horizontal bar charts with thresholds and colors
	{
		CreateScalarFunctionInfo info(TextplotBarFunction());

		FunctionDescription desc;
		desc.description = "Creates a horizontal bar chart visualization from a numeric value. "
		                   "Supports customizable width, colors, shapes (square/circle/heart), and color thresholds.";
		desc.examples = {"tp_bar(0.75)",
		                 "tp_bar(75, min := 0, max := 100, width := 20)",
		                 "tp_bar(0.5, \"on\" := '#', off := '-', width := 10)",
		                 "tp_bar(0.8, shape := 'heart', on_color := 'red')",
		                 "tp_bar(75, min := 0, max := 100, thresholds := "
		                 "[{'threshold': 80, 'color': 'red'}, "
		                 "{'threshold': 50, 'color': 'yellow'}])"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_qr: QR code generation
	{
		CreateScalarFunctionInfo info(TextplotQRFunctions());

		// duckdb_functions() matches a description to an overload on parameter_types, so a set
		// needs one fully-typed description per overload.
		for (const auto &input_type : {LogicalType::VARCHAR, LogicalType::BLOB}) {
			FunctionDescription desc;
			desc.description = "Generates a text-based QR code from a string or blob. "
			                   "Supports configurable error correction levels and custom on/off characters.";
			desc.parameter_types = {input_type, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR};
			desc.examples = {"tp_qr('https://duckdb.org')", "tp_qr('https://example.com', ecc := 'high')",
			                 "tp_qr('Hello, world!', \"on\" := '##', off := '  ')",
			                 "tp_qr('\\xDE\\xAD\\xBE\\xEF'::BLOB)"};
			info.descriptions.push_back(std::move(desc));
		}

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_density: Density plots/histograms from arrays
	{
		CreateScalarFunctionInfo info(TextplotDensityFunction());

		FunctionDescription desc;
		desc.description = "Creates a density plot (histogram) visualization from an array of numeric values. "
		                   "Supports multiple styles: shaded, dots, ascii, height, circles, safety, rainbow_circle, "
		                   "rainbow_square, moon, sparse, and white. Pass 'marker_value' to highlight the bin "
		                   "containing a particular value, optionally with a custom 'marker' character.";
		desc.examples = {"tp_density([1.0, 2.0, 2.0, 3.0, 3.0, 3.0, 4.0, 4.0, 5.0])",
		                 "tp_density([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0], width := 40)",
		                 "tp_density([1.0, 2.0, 3.0, 4.0, 5.0], style := 'height')",
		                 "tp_density([1.0, 2.0, 3.0, 4.0, 5.0], marker_value := 3.0)",
		                 "tp_density([1.0, 2.0, 3.0, 4.0, 5.0], marker := '|', marker_value := 4.0)",
		                 "tp_density([1.0, 2.0, 3.0, 4.0, 5.0], "
		                 "style := 'rainbow_square', width := 30)"};
		info.descriptions.push_back(std::move(desc));

		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		loader.RegisterFunction(std::move(info));
	}

	// tp_sparkline: Compact trend lines with multiple modes
	{
		CreateScalarFunctionInfo info(TextplotSparklineFunction());

		FunctionDescription desc;
		desc.description = "Creates a sparkline visualization from an array of numeric values. "
		                   "Supports three modes: 'absolute' (height-based), 'delta' (up/down/same direction), "
		                   "and 'trend' (direction with magnitude). Multiple themes available per mode.";
		desc.examples = {"tp_sparkline([1.0, 2.0, 3.0, 4.0, 5.0])",
		                 "tp_sparkline([1.0, 3.0, 2.0, 5.0, 4.0, 6.0], width := 20)",
		                 "tp_sparkline([1.0, 2.0, 1.0, 3.0, 2.0], "
		                 "mode := 'delta', theme := 'arrows')",
		                 "tp_sparkline([1.0, 2.0, 3.0, 4.0, 5.0], "
		                 "mode := 'absolute', theme := 'utf8_blocks')",
		                 "tp_sparkline([1.0, 5.0, 3.0, 8.0, 6.0], "
		                 "mode := 'trend', theme := 'faces')"};
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
	return "2026102002";
}

} // namespace duckdb

extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(textplot, loader) {
	duckdb::LoadInternal(loader);
}
}
