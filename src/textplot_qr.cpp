#include "textplot_qr.hpp"
#include "textplot_common.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "qrcodegen.hpp"

namespace duckdb {

struct TextplotQRBindData : public FunctionData {
	string ecc = "low";
	string on = "";
	string off = "";
	//! BLOB input is always encoded in byte mode; VARCHAR may use the denser text modes.
	bool binary = false;

	TextplotQRBindData(string ecc_p, string on_p, string off_p, bool binary_p)
	    : ecc(std::move(ecc_p)), on(std::move(on_p)), off(std::move(off_p)), binary(binary_p) {
	}

	unique_ptr<FunctionData> Copy() const override;
	bool Equals(const FunctionData &other_p) const override;
};

unique_ptr<FunctionData> TextplotQRBindData::Copy() const {
	return make_uniq<TextplotQRBindData>(ecc, on, off, binary);
}

bool TextplotQRBindData::Equals(const FunctionData &other_p) const {
	const auto &other = other_p.Cast<TextplotQRBindData>();
	return ecc == other.ecc && on == other.on && off == other.off && binary == other.binary;
}

unique_ptr<FunctionData> TextplotQRBind(BindScalarFunctionInput &input) {
	auto &arguments = input.GetArguments();
	if (arguments[0]->GetReturnType().id() == LogicalTypeId::UNKNOWN) {
		throw ParameterNotResolvedException();
	}
	const auto binary = arguments[0]->GetReturnType() == LogicalType::BLOB;
	if (!binary && arguments[0]->GetReturnType() != LogicalType::VARCHAR) {
		throw InvalidTypeException("tp_qr first argument must be a VARCHAR or BLOB");
	}

	// The options are keyword-only parameters of the signature (see TextplotQRFunctions): DuckDB has
	// already matched the names, cast the values to VARCHAR and filled in the defaults. An explicit
	// NULL option makes the whole call NULL before bind is reached (default NULL handling).
	const auto ecc = input.GetNonNullConstant("ecc").GetValue<string>();
	auto on = input.GetNonNullConstant("on").GetValue<string>();
	auto off = input.GetNonNullConstant("off").GetValue<string>();
	TextplotValidateCellString("tp_qr", "on", on);
	TextplotValidateCellString("tp_qr", "off", off);

	// Validate ECC at bind time
	if (ecc != "low" && ecc != "medium" && ecc != "quartile" && ecc != "high") {
		throw BinderException("tp_qr: 'ecc' argument must be one of 'low', 'medium', 'quartile', 'high'");
	}

	if (off.empty()) {
		off = "⬜";
	}

	if (on.empty()) {
		on = "⬛";
	}

	return make_uniq<TextplotQRBindData>(ecc, on, off, binary);
}

void TextplotQR(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &value_vector = args.data[0];
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.BindInfo()->Cast<TextplotQRBindData>();

	UnaryExecutor::Execute<string_t, string_t>(value_vector, result, [&](string_t value) {
		// std::vector<qrcodegen::QrSegment> segs = qrcodegen::QrSegment::makeSegments(value.GetData());

		// auto qr = qrcodegen::QrCode::encodeSegments(segs, qrcodegen::QrCode::Ecc::LOW, bind_data.min_version,
		//                                             bind_data.max_version, -1, bind_data.boost_ecc);

		// ECC is already validated at bind time
		qrcodegen::QrCode::Ecc ecc_level = qrcodegen::QrCode::Ecc::LOW;
		if (bind_data.ecc == "medium") {
			ecc_level = qrcodegen::QrCode::Ecc::MEDIUM;
		} else if (bind_data.ecc == "quartile") {
			ecc_level = qrcodegen::QrCode::Ecc::QUARTILE;
		} else if (bind_data.ecc == "high") {
			ecc_level = qrcodegen::QrCode::Ecc::HIGH;
		}

		const auto text = value.GetString();

		// encodeText takes a NUL-terminated string, so it would silently drop everything after an
		// embedded NUL. Use byte mode for those inputs (and for BLOB) to encode the whole payload;
		// plain text keeps the denser numeric/alphanumeric segment modes.
		const auto use_binary = bind_data.binary || text.find('\0') != std::string::npos;
		const auto qr =
		    use_binary ? qrcodegen::QrCode::encodeBinary(std::vector<std::uint8_t>(text.begin(), text.end()), ecc_level)
		               : qrcodegen::QrCode::encodeText(text.c_str(), ecc_level);

		const auto size = qr.getSize();
		string result_str;
		result_str.reserve(static_cast<size_t>(size) *
		                   (static_cast<size_t>(size) * std::max(bind_data.on.size(), bind_data.off.size()) + 1));
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				result_str += qr.getModule(x, y) ? bind_data.on : bind_data.off;
			}
			result_str += "\n";
		}
		return StringVector::AddString(result, result_str);
	});
}

ScalarFunctionSet TextplotQRFunctions() {
	ScalarFunctionSet set("tp_qr");
	for (const auto &input_type : {LogicalType::VARCHAR, LogicalType::BLOB}) {
		ScalarFunction function("tp_qr", vector<LogicalType> {}, LogicalType::VARCHAR, TextplotQR, TextplotQRBind);
		// tp_qr(data, *, ecc, on, off)
		function.GetSignature()
		    .AddPositionalOnly("data", input_type)
		    .AddKeywordOnly("ecc", LogicalType::VARCHAR, Value("low"))
		    // An empty 'on'/'off' string means the default black/white square.
		    .AddKeywordOnly("on", LogicalType::VARCHAR, Value(""))
		    .AddKeywordOnly("off", LogicalType::VARCHAR, Value(""));
		set.AddFunction(std::move(function));
	}
	// QR encoding raises an error when the payload is too long for any QR code version.
	set.SetFallible();
	return set;
}

} // namespace duckdb
