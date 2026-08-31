#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types/vector.hpp"
#include <vector>

namespace duckdb {

//! Upper bound on the number of cells a textplot function may emit for a single value.
//! string_t stores its length in a uint32_t, so an unbounded 'width' can overflow it and
//! produce a string that is cut in the middle of a multi-byte character.
static constexpr int64_t TEXTPLOT_MAX_WIDTH = 1000000;

//! Upper bound, in bytes, on a single user-supplied cell string ('on'/'off'). Combined with
//! TEXTPLOT_MAX_WIDTH (and with the largest possible QR code) this keeps every result well
//! below the uint32_t length limit described above.
static constexpr idx_t TEXTPLOT_MAX_CELL_BYTES = 256;

//! Validates the first argument of a list-taking textplot function at bind time.
//! Accepts a list of numerics, and also a list whose child type is SQLNULL: that is the type
//! DuckDB infers for the `[]` literal and for lists containing only NULL.
//! Throws ParameterNotResolvedException for an unresolved prepared statement parameter.
bool TextplotIsNumericList(const LogicalType &type);

//! Validates 'width' at bind time, raising a BinderException when out of range.
void TextplotValidateWidth(const string &function_name, int64_t width);

//! Validates a user-supplied cell string at bind time, raising a BinderException when too long.
void TextplotValidateCellString(const string &function_name, const string &argument_name, const string &value);

//! Checked, NULL-aware access to the DOUBLE child of a list vector.
//!
//! Reading a list child requires more than FlatVector::GetData: the child may be a dictionary or
//! constant vector (whose data pointer does not line up with the list offsets), and its validity
//! mask must be consulted or NULL slots are read as uninitialized memory.
class TextplotListReader {
public:
	TextplotListReader(ClientContext &context, Vector &source, idx_t count);

	//! The LIST(DOUBLE) vector to drive the executor with.
	Vector &GetVector() {
		return list_vector;
	}

	//! Replaces `out` with the finite, non-NULL values of `entry`. NULL, NaN and infinite
	//! elements are skipped, so `out` may be shorter than entry.length (or empty).
	void Extract(const list_entry_t &entry, std::vector<double> &out) const;

private:
	//! Declared first so that it outlives child_format, which points into its buffers.
	Vector list_vector;
	idx_t child_count;
	UnifiedVectorFormat child_format;
	const double *child_data;
};

} // namespace duckdb
