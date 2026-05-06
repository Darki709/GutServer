
#include "yfinance_fetcher.hpp"

#include <curl/curl.h>
#include "../external/sqlite3.h"

// ── nlohmann/json ────────────────────────────────────────────────────────────
#include "../../json.hpp"
#include "price_data_db_helper.hpp"

#define API_ENDPOINT "https://query1.finance.yahoo.com/v8/finance/chart/"

namespace Gut
{
	using json = nlohmann::json;


	// ═══════════════════════════════════════════════════════════════════════════════
	//  libcurl helpers
	// ═══════════════════════════════════════════════════════════════════════════════

	static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
	{
		auto *buf = static_cast<std::string *>(userdata);
		buf->append(ptr, size * nmemb);
		return size * nmemb;
	}

	// URL-encode a single query-parameter value
	static std::string url_encode(CURL *curl, const std::string &value)
	{
		char *encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
		std::string result(encoded);
		curl_free(encoded);
		return result;
	}

	struct HttpResponse
	{
		long status_code = 0;
		std::string body;
	};

	/**
	 * Perform a blocking GET request.
	 * Returns HTTP status + body; throws std::runtime_error on libcurl failure.
	 */
	static HttpResponse http_get(const std::string &url,
								 const std::vector<std::pair<std::string, std::string>> &params,
								 const std::vector<std::string> &header_lines,
								 long timeout_sec = 10)
	{
		CURL *curl = curl_easy_init();
		if (!curl)
			throw std::runtime_error("curl_easy_init() failed");

		// Build query string
		std::string full_url = url;
		if (!params.empty())
		{
			full_url += '?';
			for (size_t i = 0; i < params.size(); ++i)
			{
				if (i)
					full_url += '&';
				full_url += params[i].first + '=' + url_encode(curl, params[i].second);
			}
		}

		HttpResponse resp;

		// Headers
		struct curl_slist *headers = nullptr;
		for (const auto &h : header_lines)
			headers = curl_slist_append(headers, h.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		// Disable peer-cert verification only if you have cert-bundle issues on Windows;
		// leave enabled on production Linux builds.
		// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		CURLcode rc = curl_easy_perform(curl);
		if (rc != CURLE_OK)
		{
			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
			throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(rc));
		}

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return resp;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	//  JSON parsing
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Safely reads a JSON array element as double.
	 * Returns false (and leaves `out` untouched) if the element is null/missing.
	 */
	static bool safe_double(const json &arr, std::size_t idx, double &out)
	{
		if (idx >= arr.size())
			return false;
		const auto &v = arr[idx];
		if (v.is_null())
			return false;
		out = v.get<double>();
		return true;
	}

	static bool safe_int64(const json &arr, std::size_t idx, long long &out)
	{
		if (idx >= arr.size())
			return false;
		const auto &v = arr[idx];
		if (v.is_null())
			return false;
		out = v.get<long long>();
		return true;
	}

	/**
	 * Parse Yahoo Finance v8 JSON into a flat vector of OHLCVRow.
	 * Returns empty vector on any structural error.
	 */
	static std::vector<OHLCVRow> parse_yahoo_json(const std::string &body)
	{
		std::vector<OHLCVRow> rows;

		json root;
		try
		{
			root = json::parse(body);
		}
		catch (...)
		{
			return rows;
		}

		// chart.result[0]
		auto &chart = root["chart"];
		if (!chart.is_object())
			return rows;

		auto &result_arr = chart["result"];
		if (!result_arr.is_array() || result_arr.empty())
			return rows;

		auto &result = result_arr[0];

		auto &timestamps_j = result["timestamp"];
		if (!timestamps_j.is_array() || timestamps_j.empty())
			return rows;

		auto &indicators = result["indicators"];
		if (!indicators.is_object())
			return rows;

		auto &quote_arr = indicators["quote"];
		if (!quote_arr.is_array() || quote_arr.empty())
			return rows;

		auto &quote = quote_arr[0];

		// Fetch arrays (may be absent → null json node → treat as all-null)
		const json &opens = quote.value("open", json::array());
		const json &highs = quote.value("high", json::array());
		const json &lows = quote.value("low", json::array());
		const json &closes = quote.value("close", json::array());
		const json &vols = quote.value("volume", json::array());

		std::size_t n = timestamps_j.size();
		rows.reserve(n);

		for (std::size_t i = 0; i < n; ++i)
		{
			if (timestamps_j[i].is_null())
				continue;

			double o, h, l, c;
			if (!safe_double(opens, i, o))
				continue; // required
			if (!safe_double(closes, i, c))
				continue; // required
			if (!safe_double(highs, i, h))
				h = o;
			if (!safe_double(lows, i, l))
				l = o;

			long long vol = 0;
			safe_int64(vols, i, vol);

			OHLCVRow row;
			row.timestamp = static_cast<std::time_t>(timestamps_j[i].get<long long>());
			row.open = o;
			row.high = h;
			row.low = l;
			row.close = c;
			row.volume = vol;
			rows.push_back(row);
		}

		return rows;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	//  Public API
	// ═══════════════════════════════════════════════════════════════════════════════

	int fetch_live_data(const std::string &ticker,
						int interval_sec,
						const std::string &db_path)
	{
		// ── 1. Open DB (once per call; for a long-running service consider
		//        passing an already-open sqlite3* instead) ─────────────────────
		Price_data_db_helper db;

		const std::string interval = seconds_to_interval(interval_sec);

		// ── 2. Determine fetch range ─────────────────────────────────────────
		std::time_t last_ts = 0;
		try
		{
			last_ts = db.get_last_fetch_time( ticker, interval);
		}
		catch (...)
		{ /* treat as "never fetched" */
		}

		std::vector<std::pair<std::string, std::string>> params;
		params.push_back({"interval", interval});
		params.push_back({"includePrePost", "false"});

		if (interval == "1m")
		{
			params.push_back({"range", "1d"});
		}
		else if (last_ts == 0)
		{
			params.push_back({"range", "max"});
		}
		else
		{
			params.push_back({"period1", std::to_string(static_cast<long long>(last_ts))});
			params.push_back({"period2", std::to_string(static_cast<long long>(std::time(nullptr)))});
		}

		// ── 3. HTTP GET ───────────────────────────────────────────────────────
		const std::string url =
			API_ENDPOINT + ticker;

		const std::vector<std::string> req_headers = {
			"User-Agent: Mozilla/5.0 (compatible; StockFetcher/1.0)",
			"Accept: application/json",
		};

		HttpResponse resp;
		try
		{
			resp = http_get(url, params, req_headers, /*timeout_sec=*/15);
		}
		catch (const std::exception &e)
		{
			std::cerr << "[fetch_live_data] HTTP error: " << e.what() << '\n';
			return -1;
		}

		if (resp.status_code != 200)
		{
			std::cerr << "[fetch_live_data] HTTP " << resp.status_code
					  << " for " << ticker << '\n';
			return -1;
		}

		// ── 4. Parse JSON ─────────────────────────────────────────────────────
		std::vector<OHLCVRow> rows = parse_yahoo_json(resp.body);
		if (rows.empty())
		{
			std::cerr << "[fetch_live_data] No usable rows for " << ticker
					  << " " << interval << '\n';
			return -1;
		}

		// ── 5. Insert into DB ─────────────────────────────────────────────────
		int inserted = db.insert_price_data(ticker, interval, rows);
		if (inserted < 0)
			return -2;

		return 0;
	}
}