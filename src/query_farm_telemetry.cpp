#include "query_farm_telemetry.hpp"
#include "duckdb.hpp"
#include "duckdb/common/http_util.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "duckdb/storage/object_cache.hpp"
#include "yyjson.hpp"
#include <cstdlib>
#include <thread>

using namespace duckdb_yyjson; // NOLINT

namespace duckdb {

namespace {

const char *const TELEMETRY_URL = "https://duckdb-in.query-farm.services/";

// Performs the POST. Everything this touches -- the database's HTTPUtil and the
// HTTPParams built on the loading thread -- must outlive the call. The owner
// below guarantees that by joining the worker before the database is torn down.
void SendTelemetryRequest(DatabaseInstance &db, HTTPParams &params, const string &body) {
	try {
		HTTPHeaders headers;
		headers.Insert("Content-Type", "application/json");
		PostRequestInfo post_request(TELEMETRY_URL, headers, params, const_data_ptr_cast(body.data()), body.size());
		auto &http_util = HTTPUtil::Get(db);
		auto response = http_util.Request(post_request);
	} catch (...) {
		// Telemetry is best-effort: ignore all errors.
	}
}

#ifndef __EMSCRIPTEN__
// Owns the background thread that sends the telemetry ping.
//
// Lifetime matters here. A plain detached thread holding a shared_ptr to the
// DatabaseInstance could outlive the DuckDB object that created it: the process
// then reaches exit() and starts running static destructors (DuckDB globals,
// httpfs, OpenSSL) while the thread is still inside the HTTP request -- or the
// thread drops the last reference and runs ~DatabaseInstance concurrently with
// that teardown. Either way it segfaults on shutdown (issue #4).
//
// Storing the worker in the database's ObjectCache ties it to the database
// instead: ~DatabaseInstance destroys the cache before the scheduler, buffer
// manager, and log manager, so the join below happens while the database is
// still intact and the thread can never outlive it.
class QueryFarmTelemetryWorker : public ObjectCacheEntry {
public:
	QueryFarmTelemetryWorker(DatabaseInstance &db, unique_ptr<HTTPParams> params, string body)
	    : db(db), params(std::move(params)), body(std::move(body)) {
	}

	~QueryFarmTelemetryWorker() override {
		if (worker.joinable()) {
			worker.join();
		}
	}

	void Start() {
		try {
			worker = std::thread([this]() { SendTelemetryRequest(db, *params, body); });
		} catch (...) {
			// Could not spawn a thread; drop the ping.
		}
	}

	static string ObjectType() {
		return "query_farm_telemetry_worker";
	}

	string GetObjectType() override {
		return ObjectType();
	}

	// Not evictable: an eviction would destroy this entry (and join the thread)
	// at an arbitrary moment. The entry must live exactly as long as the database.
	optional_idx GetEstimatedCacheMemory() const override {
		return optional_idx();
	}

private:
	DatabaseInstance &db;
	unique_ptr<HTTPParams> params;
	string body;
	std::thread worker;
};
#endif

string BuildTelemetryBody(const string &extension_name, const string &extension_version) {
	auto doc = yyjson_mut_doc_new(nullptr);
	auto result_obj = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, result_obj);

	auto platform = DuckDB::Platform();

	yyjson_mut_obj_add_str(doc, result_obj, "extension_name", extension_name.c_str());
	yyjson_mut_obj_add_str(doc, result_obj, "extension_version", extension_version.c_str());
	yyjson_mut_obj_add_str(doc, result_obj, "user_agent", "query-farm/20260201");
	yyjson_mut_obj_add_str(doc, result_obj, "duckdb_platform", platform.c_str());
	yyjson_mut_obj_add_str(doc, result_obj, "duckdb_library_version", DuckDB::LibraryVersion());
	yyjson_mut_obj_add_str(doc, result_obj, "duckdb_release_codename", DuckDB::ReleaseCodename());
	yyjson_mut_obj_add_str(doc, result_obj, "duckdb_source_id", DuckDB::SourceID());

	size_t telemetry_len = 0;
	auto telemetry_data =
	    yyjson_mut_val_write_opts(result_obj, YYJSON_WRITE_ALLOW_INF_AND_NAN, NULL, &telemetry_len, nullptr);
	yyjson_mut_doc_free(doc);

	if (telemetry_data == nullptr) {
		throw SerializationException("Failed to serialize telemetry data.");
	}
	string body(telemetry_data, telemetry_len);
	free(telemetry_data);
	return body;
}

} // namespace

INTERNAL_FUNC void QueryFarmSendTelemetry(ExtensionLoader &loader, const string &extension_name,
                                          const string &extension_version) {
	const char *opt_out = std::getenv("QUERY_FARM_TELEMETRY_OPT_OUT");
	if (opt_out != nullptr) {
		return;
	}

	auto &db = loader.GetDatabaseInstance();
	try {
		ExtensionHelper::TryAutoLoadExtension(db, "httpfs");
	} catch (...) {
		return;
	}
	if (!db.ExtensionIsLoaded("httpfs")) {
		return;
	}

	auto body = BuildTelemetryBody(extension_name, extension_version);

	// Resolve settings, proxy, and secrets on the loading thread, so the worker
	// touches nothing but the HTTP client. Telemetry is best-effort: bound the
	// request so a stalled connection cannot hold up database shutdown for long,
	// and never retry -- a dropped ping is not worth one.
	auto &http_util = HTTPUtil::Get(db);
	unique_ptr<HTTPParams> params = http_util.InitializeParameters(db, TELEMETRY_URL);
	params->timeout = 5;
	params->timeout_usec = 0;
	params->retries = 0;

#ifndef __EMSCRIPTEN__
	auto worker = make_shared_ptr<QueryFarmTelemetryWorker>(db, std::move(params), std::move(body));
	// Register before starting: if the cache rejected the entry, no thread would
	// exist that the database does not own.
	db.GetObjectCache().Put("query_farm_telemetry-" + extension_name, worker);
	worker->Start();
#else
	SendTelemetryRequest(db, *params, body);
#endif
}

} // namespace duckdb
