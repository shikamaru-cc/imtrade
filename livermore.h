#ifndef LIVERMORE_H
#define LIVERMORE_H

#include <time.h>
#include <stdint.h>

typedef struct lv_candles {
    time_t *timestamp;
    double *open;
    double *high;
    double *low;
    double *close;
    uint64_t *volume;
    size_t size;
} lv_candles;

extern void lv_candles_init (lv_candles *candles, size_t sz);
extern void lv_candles_free (lv_candles *candles);
extern int  lv_candles_fetch(lv_candles *candles, const char *market, const char *symbol, const char *interval);

#endif //LIVERMORE_H
