// yfinance_fetcher.cpp

#include "yfinance_fetcher.hpp"

#include <curl/curl.h>
#include "../external/sqlite3.h"
#include "../../json.hpp"

#define API_ENDPOINT "https://query1.finance.yahoo.com/v8/finance/chart/"

namespace Gut
{
    using json = nlohmann::json;

    // ═════════════════════════════════════════════════════════════════════════
    //  libcurl helpers
    // ═════════════════════════════════════════════════════════════════════════

    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        auto* buf = static_cast<std::string*>(userdata);
        buf->append(ptr, size * nmemb);
        return size * nmemb;
    }

    static std::string url_encode(CURL* curl, const std::string& value)
    {
        char* encoded = curl_easy_escape(curl, value.c_str(),
                                         static_cast<int>(value.size()));
        std::string result(encoded);
        curl_free(encoded);
        return result;
    }

    struct HttpResponse
    {
        long        status_code = 0;
        std::string body;
    };

    static HttpResponse http_get(
        const std::string& url,
        const std::vector<std::pair<std::string,std::string>>& params,
        const std::vector<std::string>& header_lines,
        long timeout_sec = 15)
    {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("curl_easy_init() failed");

        std::string full_url = url;
        if (!params.empty())
        {
            full_url += '?';
            for (size_t i = 0; i < params.size(); ++i)
            {
                if (i) full_url += '&';
                full_url += params[i].first + '=' +
                            url_encode(curl, params[i].second);
            }
        }

        HttpResponse resp;
        struct curl_slist* headers = nullptr;
        for (const auto& h : header_lines)
            headers = curl_slist_append(headers, h.c_str());

        curl_easy_setopt(curl, CURLOPT_URL,            full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp.body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        timeout_sec);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        std::cout << "[HTTP] GET " << full_url << '\n';

        CURLcode rc = curl_easy_perform(curl);
        if (rc != CURLE_OK)
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error(std::string("curl: ") + curl_easy_strerror(rc));
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return resp;
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  JSON helpers
    // ═════════════════════════════════════════════════════════════════════════

    static bool safe_double(const json& arr, std::size_t idx, double& out)
    {
        if (idx >= arr.size()) return false;
        const auto& v = arr[idx];
        if (v.is_null()) return false;
        out = v.get<double>();
        return true;
    }

    static bool safe_int64(const json& arr, std::size_t idx, long long& out)
    {
        if (idx >= arr.size()) return false;
        const auto& v = arr[idx];
        if (v.is_null()) return false;
        out = v.get<long long>();
        return true;
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  Timestamp normalisation
    //
    //  DAILY bars
    //  ----------
    //  Yahoo 1d timestamps are midnight UTC of the trading day
    //  (e.g. 2023-09-01 00:00:00 UTC = 1693526400).  We keep them exactly as
    //  midnight UTC.  The PRIMARY KEY on (ticker, interval, date) ensures the
    //  same calendar day always maps to the same value across fetches.
    //
    //  The previous "noon UTC" approach called timegm()/_mkgmtime() which is
    //  unavailable on some Windows SDKs and was corrupting timestamps by
    //  adding 12 hours, turning midnight into noon and producing a non-86400s
    //  stride between daily bars (visible as huge gaps in the DB).
    //
    //  INTRADAY bars  (1m / 5m / 15m / 1h)
    //  -------------------------------------
    //  Yahoo occasionally returns a bar at e.g. 21:33 for a 15m interval
    //  (valid boundaries are :00 :15 :30 :45).  We snap each timestamp down
    //  to the nearest clean multiple via integer floor division, making the
    //  PRIMARY KEY stable across fetches and aligning bars for charting.
    // ═════════════════════════════════════════════════════════════════════════

    static std::time_t snap_to_interval(std::time_t raw, int interval_sec)
    {
        if (interval_sec >= 86400) return raw;       // daily — keep as-is
        return (raw / interval_sec) * interval_sec;  // floor to boundary
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  Parse Yahoo Finance v8 JSON response
    // ═════════════════════════════════════════════════════════════════════════

    static std::vector<StockData> parse_yahoo_json(const std::string& body,
                                                    const std::string& interval,
                                                    int interval_sec)
    {
        std::vector<StockData> rows;

        json root;
        try { root = json::parse(body); }
        catch (const std::exception& e)
        {
            std::cerr << "[PARSE] JSON parse failed: " << e.what() << '\n';
            return rows;
        }

        auto& chart = root["chart"];
        if (!chart.is_object()) { std::cerr << "[PARSE] Missing 'chart'\n"; return rows; }

        // Check for API-level error message
        auto& err = chart["error"];
        if (!err.is_null())
        {
            std::cerr << "[PARSE] Yahoo API error: " << err.dump() << '\n';
            return rows;
        }

        auto& result_arr = chart["result"];
        if (!result_arr.is_array() || result_arr.empty())
        {
            std::cerr << "[PARSE] chart.result is empty\n";
            return rows;
        }

        auto& result      = result_arr[0];
        auto& timestamps_j = result["timestamp"];
        if (!timestamps_j.is_array() || timestamps_j.empty())
        {
            std::cerr << "[PARSE] No timestamps in response\n";
            return rows;
        }

        auto& indicators = result["indicators"];
        if (!indicators.is_object()) { std::cerr << "[PARSE] Missing indicators\n"; return rows; }

        auto& quote_arr = indicators["quote"];
        if (!quote_arr.is_array() || quote_arr.empty())
        {
            std::cerr << "[PARSE] Missing quote array\n";
            return rows;
        }

        auto& quote = quote_arr[0];

        const json& opens  = quote.value("open",   json::array());
        const json& highs  = quote.value("high",   json::array());
        const json& lows   = quote.value("low",    json::array());
        const json& closes = quote.value("close",  json::array());
        const json& vols   = quote.value("volume", json::array());

        std::size_t n         = timestamps_j.size();
        int         skipped   = 0;
        rows.reserve(n);

        for (std::size_t i = 0; i < n; ++i)
        {
            if (timestamps_j[i].is_null()) { ++skipped; continue; }

            double o = 0, h = 0, l = 0, c = 0;
            if (!safe_double(opens,  i, o)) { ++skipped; continue; }
            if (!safe_double(closes, i, c)) { ++skipped; continue; }
            if (!safe_double(highs,  i, h)) h = o;
            if (!safe_double(lows,   i, l)) l = o;

            long long vol = 0;
            safe_int64(vols, i, vol);

            std::time_t raw_ts = static_cast<std::time_t>(
                timestamps_j[i].get<long long>());
            std::time_t norm_ts = snap_to_interval(raw_ts, interval_sec);

            StockData row{};
            row.ts     = static_cast<uint64_t>(norm_ts);
            row.open   = o;
            row.high   = h;
            row.low    = l;
            row.close  = c;
            row.volume = static_cast<uint64_t>(vol);
            rows.push_back(row);
        }

        std::cout << "[PARSE] " << rows.size() << " valid candles ("
                  << skipped << " skipped) for interval=" << interval << '\n';
        return rows;
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  Per-interval API range rules
    //
    //  Yahoo restricts how much history each interval can return in one call.
    //  When we have no prior history we request the maximum the API allows.
    //  When we have prior history we use period1/period2.
    //
    //  Interval | Max single-call range
    //  ---------+-----------------------
    //  1m       | 7 days
    //  5m       | 60 days
    //  15m      | 60 days
    //  1h       | 730 days (~2 years)
    //  1d       | max (full history)
    // ═════════════════════════════════════════════════════════════════════════

    static std::string max_range_for_interval(const std::string& iv)
    {
        if (iv == "1m")  return "7d";
        if (iv == "5m")  return "60d";
        if (iv == "15m") return "60d";
        if (iv == "1h")  return "730d";
        return "10y"; // 1d — "max" returns quarterly-aggregated data for old history
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  fetch_price_data  — public entry point
    // ═════════════════════════════════════════════════════════════════════════

    int YFinance_fetcher::fetch_price_data(String& ticker, Interval interval_enum)
    {
        const std::string interval =
            seconds_to_interval(static_cast<int>(interval_enum));

        std::cout << "[FETCH] === fetch_price_data: "
                  << ticker << " [" << interval << "] ===\n";

        // ── 1. DB: get last candle timestamp ──────────────────────────────
        Price_data_db_helper db;
        uint64_t last_ts = 0;
        try { last_ts = db.get_last_fetch_time(ticker, interval); }
        catch (...) { /* treat as never fetched */ }

        // ── 2. Build request parameters ───────────────────────────────────
        std::vector<std::pair<std::string,std::string>> params;
        params.push_back({"interval",       interval});
        params.push_back({"includePrePost", "false"});

        if (last_ts == 0)
        {
            // No prior data — request maximum available range
            const std::string range = max_range_for_interval(interval);
            params.push_back({"range", range});
            std::cout << "[FETCH] No history, requesting range=" << range << '\n';
        }
        else
        {
            // Advance by one interval so we never re-fetch the last stored candle.
            // This is the key fix for the "duplicate daily bar on reload" problem.
            uint64_t period1 = last_ts + static_cast<uint64_t>(interval_enum);
            uint64_t period2 = static_cast<uint64_t>(std::time(nullptr));

            // Sanity check: if period1 is in the future there is nothing new.
            if (period1 >= period2)
            {
                std::cout << "[FETCH] Data is already up-to-date for "
                          << ticker << " [" << interval << "]. Nothing to do.\n";
                return 0;
            }

            params.push_back({"period1", std::to_string(period1)});
            params.push_back({"period2", std::to_string(period2)});

            // Format timestamps for the log
            auto fmt = [](uint64_t ts) -> std::string {
                std::time_t t = static_cast<std::time_t>(ts);
                char buf[32];
                struct tm lt{};
#ifdef _WIN32
                localtime_s(&lt, &t);
#else
                localtime_r(&t, &lt);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
                return std::string(buf);
            };

            std::cout << "[FETCH] Incremental fetch: period1="
                      << fmt(period1) << "  period2=" << fmt(period2) << '\n';
        }

        // ── 3. HTTP GET ───────────────────────────────────────────────────
        const std::string url = std::string(API_ENDPOINT) + ticker;

        const std::vector<std::string> req_headers = {
            "User-Agent: Mozilla/5.0 (compatible; StockFetcher/1.0)",
            "Accept: application/json",
        };

        HttpResponse resp;
        try { resp = http_get(url, params, req_headers, 15); }
        catch (const std::exception& e)
        {
            std::cerr << "[FETCH] HTTP error: " << e.what() << '\n';
            return -1;
        }

        std::cout << "[FETCH] HTTP " << resp.status_code
                  << ", body size=" << resp.body.size() << " bytes\n";

        if (resp.status_code != 200)
        {
            std::cerr << "[FETCH] Non-200 response for " << ticker << '\n';
            // Print a snippet of the body to help diagnose rate-limiting etc.
            std::cerr << "[FETCH] Body (first 512 chars): "
                      << resp.body.substr(0, 512) << '\n';
            return -1;
        }

        // ── 4. Parse ──────────────────────────────────────────────────────
        std::vector<StockData> rows = parse_yahoo_json(resp.body, interval, static_cast<int>(interval_enum));
        if (rows.empty())
        {
            std::cout << "[FETCH] No new candles for "
                      << ticker << " [" << interval << "]. Returning -1.\n";
            return -1;
        }

        std::cout << "[FETCH] Parsed " << rows.size() << " candles. "
                  << "First ts=" << rows.front().ts
                  << "  Last ts=" << rows.back().ts << '\n';

        // ── 5. Insert into DB ─────────────────────────────────────────────
        int inserted = db.insert_price_data(ticker, interval, rows);
        if (inserted < 0)
        {
            std::cerr << "[FETCH] DB insert failed.\n";
            return -2;
        }

        std::cout << "[FETCH] Done. Inserted/updated " << inserted
                  << " rows for " << ticker << " [" << interval << "]\n";
        return 0;
    }

} // namespace Gut