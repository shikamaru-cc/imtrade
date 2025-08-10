#ifndef IMTRADE_DATABASE_H
#define IMTRADE_DATABASE_H

#include <vector>
#include <string>

bool database_init();

typedef struct candle {
    time_t timestamp;
    double open;
    double high;
    double low;
    double close;
    uint64_t volume;
} candle_t;

std::vector<candle_t> candles_get(const std::string & symbol, const std::string & interval, int * limit);
void                  candles_put(const std::string & symbol, const std::string & interval, const std::vector<candle_t> & candles);

#endif //IMTRADE_DATABASE_H