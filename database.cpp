#include "database.h"
#include <stdio.h>
#include <duckdb.h>

static duckdb_database db;

#define DUCKDB_FILE "imtrade.db"

#define SQL_CREATE_CANDLES_TABLE \
    "CREATE TABLE IF NOT EXISTS candles (" \
        "symbol VARCHAR," \
        "interval VARCHAR," \
        "timestamp TIMESTAMP," \
        "open FLOAT," \
        "high FLOAT," \
        "low FLOAT," \
        "close FLOAT," \
        "volume BIGINT," \
        "PRIMARY KEY (symbol, interval, timestamp)" \
    ");"

bool database_init()
{
    if (duckdb_open(DUCKDB_FILE, &db) == DuckDBError) {
        fprintf(stderr, "Failed to open database\n");
        return false;
    }

    duckdb_connection conn;
    if (duckdb_connect(db, &conn) == DuckDBError) {
        fprintf(stderr, "Failed to open duckdb connection\n");
        duckdb_disconnect(&conn);
        return false;
    }

    duckdb_result res;
    if (duckdb_query(conn, SQL_CREATE_CANDLES_TABLE, &res) == DuckDBError) {
        fprintf(stderr, "Failed to create database: %s\n", duckdb_result_error(&res));
        duckdb_disconnect(&conn);
        duckdb_destroy_result(&res);
        return false;
    }

    duckdb_disconnect(&conn);
    duckdb_destroy_result(&res);
    return true;
}

static bool candles_put_db(
    duckdb_connection conn,
    const std::string & symbol,
    const std::string & interval,
    const std::vector<candle_t> & candles
) {
    duckdb_appender appender;
    duckdb_state st = DuckDBSuccess;
    st = duckdb_appender_create(conn, NULL, "candles", &appender);
    if (st == DuckDBError) return false;
    for (const auto & candle : candles) {
        duckdb_append_varchar(appender, symbol.data());
        duckdb_append_varchar(appender, interval.data());
        duckdb_append_timestamp(appender, duckdb_timestamp{candle.timestamp * 1000000});
        duckdb_append_double(appender, candle.open);
        duckdb_append_double(appender, candle.high);
        duckdb_append_double(appender, candle.low);
        duckdb_append_double(appender, candle.close);
        duckdb_append_uint64(appender, candle.volume);
        duckdb_appender_end_row(appender);
    }
    st = duckdb_appender_flush(appender);
    if (st == DuckDBError) return false;
    st = duckdb_appender_close(appender);
    if (st == DuckDBError) return false;
    return true;
}

std::vector<candle_t> candles_get(const std::string & symbol, const std::string & interval, int * limit)
{
    duckdb_connection conn;
    duckdb_state st = duckdb_connect(db, &conn);
    if (st == DuckDBError) return {};

    std::string q = "SELECT timestamp,open,high,low,close,volume FROM candles WHERE symbol=" + symbol + " AND interval=" + interval + " ORDER BY timestamp DESC;";
    duckdb_result res;
    st = duckdb_query(conn, q.data(), &res);
    if (st == DuckDBError) return {};

    std::vector<candle_t> candles;
    candles.reserve(duckdb_row_count(&res));
    for (int row = 0; row < duckdb_row_count(&res); row++) {
        candles.emplace_back(candle_t{
            duckdb_value_timestamp(&res, 0, row).micros / 1000000,
            duckdb_value_double(&res, 1, row),
            duckdb_value_double(&res, 2, row),
            duckdb_value_double(&res, 3, row),
            duckdb_value_double(&res, 4, row),
            duckdb_value_uint64(&res, 5, row),
        });
    }
    return candles;
}

void candles_put(const std::string & symbol, const std::string & interval, const std::vector<candle_t> & candles)
{
    duckdb_connection conn;
    duckdb_state st = duckdb_connect(db, &conn);
    if (st == DuckDBError) return;

    std::string q = "SELECT MAX(timestamp) FROM candles WHERE symbol='" + symbol + "' AND interval='" + interval + "';";
    duckdb_result res;
    st = duckdb_query(conn, q.data(), &res);
    if (st == DuckDBError) return;

    uint64_t curr_max_ts = 0;
    if (duckdb_row_count(&res))
        curr_max_ts = duckdb_value_timestamp(&res, 0, 0).micros / 1000000;

    duckdb_destroy_result(&res);

    std::vector<candle_t> candles_filtered;
    candles_filtered.reserve(candles.size());
    for (const auto & candle : candles) {
        if (candle.timestamp > curr_max_ts)
            candles_filtered.emplace_back(candle);
    }

    candles_put_db(conn, symbol, interval, candles_filtered);
    duckdb_disconnect(&conn);
}
