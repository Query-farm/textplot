#include "textplot_bar.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include <algorithm>
#include "qrcodegen.hpp"

namespace duckdb {

struct TextplotQRBindData : public FunctionData {
	string ecc = "low";
	string on = "";
	string off = "";

	TextplotQRBindData(string ecc_p, string on_p, string off_p)
	    : ecc(std::move(ecc_p)), on(std::move(on_p)), off(std::move(off_p)) {
	}

	unique_ptr<FunctionData> Copy() const override;
	bool Equals(const FunctionData &other_p) const override;
};

unique_ptr<FunctionData> TextplotQRBindData::Copy() const {
	return make_uniq<TextplotQRBindData>(ecc, on, off);
}

bool TextplotQRBindData::Equals(const FunctionData &other_p) const {
	return true;
}

unique_ptr<FunctionData> TextplotQRBind(ClientContext &context, ScalarFunction &bound_function,
                                        vector<unique_ptr<Expression>> &arguments) {

	if (arguments.empty()) {
		throw BinderException("tp_qr takes at least one argument");
	}

	if (!(arguments[0]->return_type == LogicalType::VARCHAR || arguments[0]->return_type == LogicalType::BLOB)) {
		throw InvalidTypeException("tp_qr first argument must a VARCHAR or BLOB");
	}

	// Optional arguments
	string ecc = "low";
	string on = "";
	string off = "";

	for (idx_t i = 1; i < arguments.size(); i++) {
		const auto &arg = arguments[i];
		if (arg->HasParameter()) {
			throw ParameterNotResolvedException();
		}
		if (!arg->IsFoldable()) {
			throw BinderException("tp_qr: arguments must be constant");
		}
		const auto &alias = arg->GetAlias();
		if (alias == "ecc") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_qr: 'ecc' argument must be a VARCHAR");
			}
			ecc = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "on") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_qr: 'on' argument must be a VARCHAR");
			}
			on = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else if (alias == "off") {
			if (arg->return_type.id() != LogicalTypeId::VARCHAR) {
				throw BinderException("tp_qr: 'off' argument must be a VARCHAR");
			}
			off = StringValue::Get(ExpressionExecutor::EvaluateScalar(context, *arg));
		} else {
			throw BinderException(StringUtil::Format("tp_qr: Unknown argument '%s'", alias));
		}
	}

	if (off.empty()) {
		off = "⬜";
	}

	if (on.empty()) {
		on = "⬛";
	}

	return make_uniq<TextplotQRBindData>(ecc, on, off);
}

void TextplotQR(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &value_vector = args.data[0];
	const auto &func_expr = state.expr.Cast<BoundFunctionExpression>();
	const auto &bind_data = func_expr.bind_info->Cast<TextplotQRBindData>();

	UnaryExecutor::Execute<string_t, string_t>(value_vector, result, args.size(), [&](string_t value) {
		// std::vector<qrcodegen::QrSegment> segs = qrcodegen::QrSegment::makeSegments(value.GetData());

		// auto qr = qrcodegen::QrCode::encodeSegments(segs, qrcodegen::QrCode::Ecc::LOW, bind_data.min_version,
		//                                             bind_data.max_version, -1, bind_data.boost_ecc);

		qrcodegen::QrCode::Ecc ecc_level = qrcodegen::QrCode::Ecc::LOW;
		if (bind_data.ecc == "low") {
			ecc_level = qrcodegen::QrCode::Ecc::LOW;
		} else if (bind_data.ecc == "medium") {
			ecc_level = qrcodegen::QrCode::Ecc::MEDIUM;
		} else if (bind_data.ecc == "quartile") {
			ecc_level = qrcodegen::QrCode::Ecc::QUARTILE;
		} else if (bind_data.ecc == "high") {
			ecc_level = qrcodegen::QrCode::Ecc::HIGH;
		} else {
			throw InvalidTypeException("tp_qr: 'ecc' argument must be one of 'low', 'medium', 'quartile', 'high'");
		}

		auto qr = qrcodegen::QrCode::encodeText(value.GetString().c_str(), ecc_level);

		string result_str;
		for (int y = 0; y < qr.getSize(); y++) {
			for (int x = 0; x < qr.getSize(); x++) {
				result_str += qr.getModule(x, y) ? bind_data.on : bind_data.off;
			}
			result_str += "\n";
		}
		return StringVector::AddString(result, result_str);
	});
}

} // namespace duckdb
