#include "textplot_extension.hpp"
#include "textplot_bar.hpp"
#include "textplot_density.hpp"
#include "textplot_sparkline.hpp"
#include "textplot_qr.hpp"
#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

static void LoadInternal(DatabaseInstance &instance) {
	auto bar_function = ScalarFunction("tp_bar", {LogicalType::DOUBLE}, LogicalType::VARCHAR, TextplotBar,
	                                   TextplotBarBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, bar_function);

	auto qr_function = ScalarFunction("tp_qr", {LogicalType::VARCHAR}, LogicalType::VARCHAR, TextplotQR, TextplotQRBind,
	                                  nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, qr_function);

	// Lets register a density function.
	auto density_function =
	    ScalarFunction("tp_density", {LogicalType::LIST(LogicalType::DOUBLE)}, LogicalType::VARCHAR, TextplotDensity,
	                   TextplotDensityBind, nullptr, nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, density_function);

	auto sparkline_function = ScalarFunction("tp_sparkline", {LogicalType::LIST(LogicalType::DOUBLE)},
	                                         LogicalType::VARCHAR, TextplotSparkline, TextplotSparklineBind, nullptr,
	                                         nullptr, nullptr, LogicalType(LogicalTypeId::ANY));
	ExtensionUtil::RegisterFunction(instance, sparkline_function);

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
