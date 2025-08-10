#include <iostream>
#include <string>
#include <iomanip>
#include <ctime>
#include "database.h"
#include <curl/curl.h>
#include "cJSON.h"

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

std::vector<candle_t > sina_parse_price(const std::string & json_str)
{
    std::cout << json_str << std::endl;
    cJSON* candles = cJSON_Parse(json_str.c_str());
    if (!candles) {
        std::cerr << "Failed to parse JSON: " << json_str << std::endl;
        return {};
    }
    std::vector<candle_t > res;
    for (int i = 0; i < cJSON_GetArraySize(candles); i++) {
        cJSON* candle = cJSON_GetArrayItem(candles, i);
        candle_t data;

        std::string date = cJSON_GetObjectItem(candle, "day")->valuestring;
        std::tm tm = {};
        std::istringstream ss(date);
        // 按照格式解析
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            std::cerr << "解析日期失败\n";
            return {};
        }
        data.timestamp = std::mktime(&tm);
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

#if 1
#include "cJSON.c"
#include "database.cpp"
int main()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    database_init();

    std::string symbol = "sh000001";
    std::string interval = "30m";
    curl_response res = sina_get_price("sh000001", "30m", 10);
    if (!res.errmsg.empty()) std::cerr << res.errmsg;
    auto candles = sina_parse_price(res.data);
    candles_put(symbol, interval, candles);
    candles = candles_get(symbol, interval, nullptr);
    for (const auto & candle : candles) {
        std::cout << candle.timestamp << ","
                  << candle.open << ","
                  << candle.high << ","
                  << candle.low << ","
                  << candle.close << std::endl;
    }
}
#endif