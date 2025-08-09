#include <iostream>
#include <string>
#include <duckdb.hpp>
#include <curl/curl.h>
#include "cJSON.h"

using namespace duckdb;

struct ohlvc_data {
    std::string date;
    time_t timestamp;
    double open;
    double high;
    double low;
    double close;
    uint64_t volume;
};

struct curl_response {
    std::string errmsg;
    std::string data;
};

size_t curl_write_cb(void* contents, size_t size, size_t nmemb, std::string* output)
{
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

curl_response sina_get_price(const std::string& code, const std::string& frequency, int count)
{
    CURL* curl = curl_easy_init();
    std::string ts = frequency.substr(0, frequency.size() - 1);
    std::string url = "http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?symbol=" + code + "&scale=" + ts + "&ma=5&datalen=" + std::to_string(count);
    curl_response response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.data);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) response.errmsg = curl_easy_strerror(res);
    curl_easy_cleanup(curl);
    return response;
}

std::vector<ohlvc_data> sina_parse_price(const std::string & json_str)
{
    cJSON* candles = cJSON_Parse(json_str.c_str());
    if (!candles) {
        std::cerr << "Failed to parse JSON: " << json_str << std::endl;
        return {};
    }
    std::vector<ohlvc_data> res;
    for (int i = 0; i < cJSON_GetArraySize(candles); i++) {
        cJSON* candle = cJSON_GetArrayItem(candles, i);
        ohlvc_data data;
        data.date = cJSON_GetObjectItem(candle, "day")->valuestring;
        data.open = std::stod(cJSON_GetObjectItem(candle, "open")->valuestring);
        data.close = std::stod(cJSON_GetObjectItem(candle, "close")->valuestring);
        data.high = std::stod(cJSON_GetObjectItem(candle, "high")->valuestring);
        data.low = std::stod(cJSON_GetObjectItem(candle, "low")->valuestring);
        data.volume = std::stoull(cJSON_GetObjectItem(candle, "volume")->valuestring);
        res.emplace_back(data);
    }
    cJSON_Delete(candles);
    return res;
}

#define SQL_CREATE_KLINES_TABLE \
    "CREATE TABLE IF NOT EXISTS stocks (" \
        "timestamp TIMESTAMP," \
        "symbol VARCHAR," \
        "open FLOAT," \
        "high FLOAT," \
        "low FLOAT," \
        "close FLOAT," \
        "volume BIGINT" \
    ");"

void duckdb_write_candles(Connection& conn, const std::string & symbol, const std::vector<ohlvc_data> & candles) {
    conn.Query(SQL_CREATE_KLINES_TABLE);
    for (const auto & candle : candles) {
        string query = "INSERT INTO stocks VALUES ('" + candle.date + "', '" + symbol + "', " +
            to_string(candle.open) + ", " + to_string(candle.high) + ", " + to_string(candle.low) + ", " +
            to_string(candle.close) + ", " + to_string(candle.volume) + ");";

            auto result = conn.Query(query);
            if (result->HasError()) {
                std::cerr << "Insert failed: " << result->GetError() << std::endl;
            }
    }
    std::cout << "Data written to DuckDB successfully." << std::endl;
}

#if 1
#include "cJSON.c"
int main() {
    // 初始化DuckDB（内存数据库）
    DuckDB db(nullptr);
    Connection conn(db);

    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 获取并写入数据
    curl_response res = sina_get_price("sh000001", "60m", 10);
    if (!res.errmsg.empty()) std::cerr << res.errmsg;
    const auto candles = sina_parse_price(res.data);
    duckdb_write_candles(conn, "sh000001", candles);

    // 查询验证
    auto result = conn.Query("SELECT * FROM stocks;");
    if (!result->HasError()) {
        result->Print();
    }

    // 清理
    curl_global_cleanup();
    return 0;
}
#endif