#include "textplot_extension.hpp"
#include "textplot_bar.hpp"
#include "textplot_density.hpp"
#include "textplot_sparkline.hpp"
#include "textplot_qr.hpp"
#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "query_farm_telemetry.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	auto bar_function = ScalarFunction("tp_bar", {LogicalType::DOUBLE}, LogicalType::VARCHAR, TextplotBar,
	                                   TextplotBarBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	loader.RegisterFunction(bar_function);

	auto qr_function = ScalarFunction("tp_qr", {LogicalType::VARCHAR}, LogicalType::VARCHAR, TextplotQR, TextplotQRBind,
	                                  nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	loader.RegisterFunction(qr_function);

	// Lets register a density function.
	auto density_function =
	    ScalarFunction("tp_density", {LogicalType::LIST(LogicalType::DOUBLE)}, LogicalType::VARCHAR, TextplotDensity,
	                   TextplotDensityBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	loader.RegisterFunction(density_function);

	auto sparkline_function = ScalarFunction("tp_sparkline", {LogicalType::LIST(LogicalType::DOUBLE)},
	                                         LogicalType::VARCHAR, TextplotSparkline, TextplotSparklineBind, nullptr,
	                                         nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	loader.RegisterFunction(sparkline_function);

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
