#include "database.h"
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
        "volume BIGINT" \
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
    duckdb_state state = DuckDBSuccess;
    state = duckdb_appender_create(conn, NULL, "candles", &appender);
    if (state == DuckDBError) return false;
    for (const auto & candle : candles) {
        duckdb_append_varchar(appender, symbol.data());
        duckdb_append_varchar(appender, interval.data());
        duckdb_append_timestamp(appender, duckdb_timestamp{candle.timestamp * 1000});
        duckdb_append_double(appender, candle.open);
        duckdb_append_double(appender, candle.high);
        duckdb_append_double(appender, candle.low);
        duckdb_append_double(appender, candle.close);
        duckdb_append_uint64(appender, candle.volume);
        duckdb_appender_end_row(appender);
    }
    duckdb_appender_flush(appender);
    duckdb_appender_close(appender);
    return true;
}

std::vector<candle_t> candles_get(const std::string & symbol, const std::string & interval, int * limit)
{
    return {};
}

void candles_put(const std::string & symbol, const std::string & interval, const std::vector<candle_t> & candles)
{
    duckdb_connection conn;
    duckdb_state st = duckdb_connect(db, &conn);
    if (st == DuckDBError) return;
    candles_put_db(conn, symbol, interval, candles);
    duckdb_disconnect(&conn);
}
