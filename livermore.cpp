#include "livermore.h"
#include "cJSON.h"
#include <stdlib.h>
#include <assert.h>
#include <curl/curl.h>

static inline time_t parse_time(const char *datetime, const char *fmt)
{
    struct tm tm = {0};
    return strptime(datetime, fmt, &tm) ? mktime(&tm) : -1;
}

static cJSON * curl_json(CURL *curl, const char *url)
{
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

void lv_candles_init(lv_candles *candles, size_t sz)
{
    candles->timestamp = (time_t *)malloc(sizeof(time_t)*sz);
    candles->open = (double *)malloc(sizeof(double)*sz);
    candles->high = (double *)malloc(sizeof(double)*sz);
    candles->low = (double *)malloc(sizeof(double)*sz);
    candles->close = (double *)malloc(sizeof(double)*sz);
    candles->volume = (uint64_t *)malloc(sizeof(uint64_t)*sz);
    candles->size = sz;
}

void lv_candles_free(lv_candles *candles)
{
    free(candles->timestamp);
    free(candles->open);
    free(candles->high);
    free(candles->low);
    free(candles->close);
    free(candles->volume);
}

#define url_fmt_sina "http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?symbol=%s&scale=%s&datalen=%lu"

int lv_candles_fetch(lv_candles *candles, const char *market, const char *symbol, const char *interval)
{
    char buf[1024];
    int rc = snprintf(buf, sizeof(buf), url_fmt_sina, symbol, interval, candles->size);
    if (rc < 0) return -1;

    CURL *curl = curl_easy_init();
    cJSON *json = curl_json(curl, buf);
    int res = 0;
    if (json) {
        res = 1;
        for (int i = 0; i < cJSON_GetArraySize(json); i++) {
            cJSON* candle = cJSON_GetArrayItem(json, i);
            char *datetime = cJSON_GetObjectItem(candle, "day")->valuestring;
            candles->timestamp[i] = parse_time(datetime, "%Y-%m-%d %H:%M:%S");
            candles->open[i] = strtod(cJSON_GetObjectItem(candle, "open")->valuestring, NULL);
            candles->close[i] = strtod(cJSON_GetObjectItem(candle, "close")->valuestring, NULL);
            candles->high[i] = strtod(cJSON_GetObjectItem(candle, "high")->valuestring, NULL);
            candles->low[i] = strtod(cJSON_GetObjectItem(candle, "low")->valuestring, NULL);
            candles->volume[i] = strtoull(cJSON_GetObjectItem(candle, "volume")->valuestring, NULL, 10);
        }
    }
    curl_easy_cleanup(curl);
    cJSON_Delete(json);
    return res;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void lv_indicator_ma(size_t winsz, size_t sz, const double *in, double *ou)
{
    assert(winsz > 0 && sz > 0 && winsz < sz);

    double sum = 0.0;
    for (int i = 0; i < winsz; i++) sum += in[i];
    ou[winsz - 1] = sum / winsz;

    // slicing window
    for (size_t i = winsz; i < sz; i++) {
        sum += in[i] - in[i - winsz];
        ou[i] = sum / winsz;
    }

    for (int i = 0; i < winsz - 1; i++) {
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