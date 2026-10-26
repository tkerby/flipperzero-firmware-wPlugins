#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YT_TITLE_SIZE 48U
#define YT_ERROR_SIZE 96U

typedef enum {
    YtParseOk,
    YtParseHttpError, /* error holds "HTTP <code>: <API message>" */
    YtParseNotFound, /* No channel for this ID/handle. */
    YtParseNoCount, /* 200 OK but no subscriberCount. */
} YtParseResult;

typedef struct {
    bool ok; /* Response parsed and the channel was found. */
    YtParseResult result;
    bool hidden; /* Channel hides its subscriber count. */
    uint64_t subscribers;
    uint64_t views;
    uint64_t videos;
    char title[YT_TITLE_SIZE];
    char error[YT_ERROR_SIZE]; /* Set for YtParseHttpError. */
} YtStats;

/* Parses a YouTube Data API v3 channels.list response (portable, no furi). */
void yt_parse_response(const char* json, uint16_t http_status, YtStats* out);

/* 1234567 -> "1,234,567" with separator ',' or "1.234.567" with '.' */
void yt_format_grouped(uint64_t value, char separator, char* out, size_t size);

/* Channel field: "UC..." id, "@handle" or "handle". Writes the URL query
 * parameter ("id=UC..." or "forHandle=%40handle"). False if invalid. */
bool yt_channel_query(const char* channel, char* out, size_t size);
/* True if the API key only contains URL-safe characters. */
bool yt_api_key_valid(const char* key);

#ifdef __cplusplus
}
#endif
