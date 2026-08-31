#include "textplot_common.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

bool TextplotIsNumericList(const LogicalType &type) {
	if (type.id() == LogicalTypeId::UNKNOWN) {
		throw ParameterNotResolvedException();
	}
	if (!type.IsNested() || type.InternalType() != PhysicalType::LIST) {
		return false;
	}
	const auto &child_type = ListType::GetChildType(type);
	return child_type.IsNumeric() || child_type.id() == LogicalTypeId::SQLNULL;
}

void TextplotValidateWidth(const string &function_name, int64_t width) {
	if (width < 1) {
		throw BinderException("%s: 'width' argument must be at least 1", function_name);
	}
	if (width > TEXTPLOT_MAX_WIDTH) {
		throw BinderException("%s: 'width' argument must be at most %d, got %d", function_name, TEXTPLOT_MAX_WIDTH,
		                      width);
	}
}

void TextplotValidateCellString(const string &function_name, const string &argument_name, const string &value) {
	if (value.size() > TEXTPLOT_MAX_CELL_BYTES) {
		throw BinderException("%s: '%s' argument must be at most %d bytes, got %d", function_name, argument_name,
		                      TEXTPLOT_MAX_CELL_BYTES, value.size());
	}
}

TextplotListReader::TextplotListReader(ClientContext &context, Vector &source, idx_t count)
    : list_vector(LogicalType::LIST(LogicalType::DOUBLE)), child_count(0), child_data(nullptr) {
	VectorOperations::Cast(context, source, list_vector, count);

	child_count = ListVector::GetListSize(list_vector);
	auto &child_vector = ListVector::GetEntry(list_vector);

	// The child may be a dictionary or constant vector; ToUnifiedFormat gives us the selection
	// vector and validity mask needed to index it correctly.
	child_vector.ToUnifiedFormat(child_count, child_format);
	child_data = UnifiedVectorFormat::GetData<double>(child_format);
}

void TextplotListReader::Extract(const list_entry_t &entry, std::vector<double> &out) const {
	out.clear();
	if (entry.length == 0) {
		return;
	}
	// Never read past the child vector, whatever offsets the list entry carries.
	if (entry.offset > child_count || entry.length > child_count - entry.offset) {
		throw InternalException("textplot: list entry [%llu, %llu) exceeds child vector of size %llu", entry.offset,
		                        entry.offset + entry.length, child_count);
	}

	out.reserve(entry.length);
	for (idx_t i = entry.offset; i < entry.offset + entry.length; i++) {
		const auto idx = child_format.sel->get_index(i);
		if (!child_format.validity.RowIsValid(idx)) {
			// NULL elements are skipped rather than read as uninitialized memory.
			continue;
		}
		const auto value = child_data[idx];
		if (!Value::DoubleIsFinite(value)) {
			// NaN and +/-Inf cannot be plotted, and casting them to an integer level is
			// undefined behaviour; drop them the same way NULL is dropped.
			continue;
		}
		out.push_back(value);
	}
}

} // namespace duckdb
