#include "livermore.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

static inline time_t parse_time(const char *datetime, const char *fmt) {
    struct tm tm = {0};
    return strptime(datetime, fmt, &tm) ? mktime(&tm) : -1;
}

static cJSON * curl_json(CURL *curl, const char *url) {
    cJSON *json = NULL;
    char *buf = NULL;
    size_t bufsz = 0;
    FILE *fs = open_memstream(&buf, &bufsz);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fs);
    CURLcode res = curl_easy_perform(curl);
    fclose(fs);
    if (res == CURLE_OK) json = cJSON_ParseWithLength(buf, bufsz);
    free(buf);
    return json;
}

static int sina_parse_interval_minutes(const char *interval) {
    if (!interval) return -1;
    char *endptr;
    long value = strtol(interval, &endptr, 10);
    if (value <= 0 || endptr == interval) return -1;
    const char *unit = endptr;
    if (!*unit || strcmp(unit, "m") == 0)
        return (int)value; // minutes
    else if (strcmp(unit, "h") == 0)
        return (int)(value * 60); // hours to minutes
    else if (strcmp(unit, "d") == 0)
        return (int)(value * 240); // days to minutes (24*60)
    return -1; // unsupported unit
}

static int sina_init_url(const char * symbol, const char * interval, size_t limit, char * res, size_t size) {
    static const char * url_fmt = "http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?symbol=%s&scale=%d&datalen=%lu";
    int interval_min = sina_parse_interval_minutes(interval);
    if (interval_min < 0 || snprintf(res, size, url_fmt, symbol, interval_min, limit) < 0)
        return -1;
    return 0;
}

static int sina_parse_result(lv_candles * candles, cJSON * json) {
    if (!cJSON_IsArray(json)) return -1;

    int array_size = cJSON_GetArraySize(json);
    if (array_size <= 0) return -1;

    // Limit to available space
    int count = min(array_size, candles->cap);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item)) continue;

        cJSON *day = cJSON_GetObjectItem(item, "day");
        cJSON *open = cJSON_GetObjectItem(item, "open");
        cJSON *high = cJSON_GetObjectItem(item, "high");
        cJSON *low = cJSON_GetObjectItem(item, "low");
        cJSON *close = cJSON_GetObjectItem(item, "close");
        cJSON *volume = cJSON_GetObjectItem(item, "volume");

        if (!cJSON_IsString(day) || !cJSON_IsString(open) ||
            !cJSON_IsString(high) || !cJSON_IsString(low) ||
            !cJSON_IsString(close) || !cJSON_IsString(volume)) {
            continue;
        }

        // Parse datetime: try full format first, then date only
        const char *day_str = cJSON_GetStringValue(day);
        candles->timestamp[i] = parse_time(day_str, "%Y-%m-%d %H:%M:%S");
        if (candles->timestamp[i] == -1) {
            // Try date-only format for daily data
            candles->timestamp[i] = parse_time(day_str, "%Y-%m-%d");
        }
        candles->open[i] = strtod(cJSON_GetStringValue(open), NULL);
        candles->high[i] = strtod(cJSON_GetStringValue(high), NULL);
        candles->low[i] = strtod(cJSON_GetStringValue(low), NULL);
        candles->close[i] = strtod(cJSON_GetStringValue(close), NULL);
        candles->volume[i] = strtoull(cJSON_GetStringValue(volume), NULL, 10);
    }

    candles->size = count;
    return 0;
}

typedef struct market_impl {
    const char* name;
    int (*init_url)(const char * symbol, const char * interval, size_t limit, char * res, size_t size);
    int (*parse_result)(lv_candles * candles, cJSON * json);
} market_impl;

static const market_impl markets[] = {
    {.name = "sina", .init_url = sina_init_url, .parse_result = sina_parse_result},
};

static const market_impl * find_market(const char * market) {
    for (size_t i = 0; i < sizeof(markets) / sizeof(markets[0]); i++) {
        if (strcmp(market, markets[i].name) == 0)
            return &markets[i];
    }
    return NULL;
}

void lv_candles_init(lv_candles *candles, size_t sz) {
    candles->timestamp = (time_t *)malloc(sizeof(time_t)*sz);
    candles->open = (double *)malloc(sizeof(double)*sz);
    candles->high = (double *)malloc(sizeof(double)*sz);
    candles->low = (double *)malloc(sizeof(double)*sz);
    candles->close = (double *)malloc(sizeof(double)*sz);
    candles->volume = (uint64_t *)malloc(sizeof(uint64_t)*sz);
    candles->size = 0;
    candles->cap = sz;
}

void lv_candles_free(lv_candles *candles) {
    free(candles->timestamp);
    free(candles->open);
    free(candles->high);
    free(candles->low);
    free(candles->close);
    free(candles->volume);
}

int lv_candles_fetch(lv_candles *candles, const char *market, const char *symbol, const char *interval) {
    if (!candles || !symbol || !interval) return -1;

    // Find market implementation
    const market_impl *impl = find_market(market);
    if (!impl) return -1;

    // Generate URL
    char url[1024];
    if (impl->init_url(symbol, interval, candles->cap, url, sizeof(url)) < 0)
        return -1;

    // Fetch data
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    cJSON *json = curl_json(curl, url);
    int result = -1;

    if (json && impl->parse_result) {
        result = impl->parse_result(candles, json);
    }

    curl_easy_cleanup(curl);
    if (json) cJSON_Delete(json);

    return result;
}

void lv_indicator_ma(size_t winsz, size_t sz, const double *in, double *ou) {
    assert(winsz > 0 && sz > 0 && winsz < sz);

    double sum = 0.0;
    for (size_t i = 0; i < winsz; i++) sum += in[i];
    ou[winsz - 1] = sum / (double)winsz;

    // slicing window
    for (size_t i = winsz; i < sz; i++) {
        sum += in[i] - in[i - winsz];
        ou[i] = sum / (double)winsz;
    }

    for (size_t i = 0; i < winsz - 1; i++) {
        ou[i] = 0.0;
    }
}

#if 0
#include "cJSON.cpp"
int main(int argc, const char *argv[])
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int sz = 100000;
    lv_candles candles;
    lv_candles_init(&candles, sz);
    // lv_candles_fetch(&candles, NULL, "sh000001", "30");
    double *ou = (double *) malloc(sizeof(double)*sz);
    if (ou == NULL) return -1;
    lv_indicator_ma(100, sz, candles.close, ou);
    lv_candles_free(&candles);
    free(ou);
    return 0;
}
#endif