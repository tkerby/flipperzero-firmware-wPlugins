#include "yt_parse.h"

#include <stdio.h>
#include <string.h>

/* Finds "key" followed by ':' and returns a pointer to the value start. */
static const char* yt_find_value(const char* json, const char* key) {
    char pattern[32];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if(written <= 0 || (size_t)written >= sizeof(pattern)) return NULL;
    const char* cursor = json;
    while((cursor = strstr(cursor, pattern)) != NULL) {
        const char* value = cursor + written;
        while(*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n')
            ++value;
        if(*value == ':') {
            ++value;
            while(*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n')
                ++value;
            return value;
        }
        cursor = value;
    }
    return NULL;
}

/* Reads a number that may be quoted ("123") as the API returns uint64 strings. */
static bool yt_read_number(const char* json, const char* key, uint64_t* out) {
    const char* value = yt_find_value(json, key);
    if(!value) return false;
    if(*value == '"') ++value;
    if(*value < '0' || *value > '9') return false;
    uint64_t result = 0;
    while(*value >= '0' && *value <= '9') {
        uint64_t digit = (uint64_t)(*value - '0');
        if(result > (UINT64_MAX - digit) / 10U) return false;
        result = result * 10U + digit;
        ++value;
    }
    *out = result;
    return true;
}

static void yt_append(char* out, size_t size, size_t* used, const char* text) {
    while(*text && *used + 1U < size)
        out[(*used)++] = *text++;
    out[*used] = '\0';
}

/* German umlauts to ASCII; anything else outside ASCII becomes '?'. */
static const char* yt_latin1_ascii(unsigned code) {
    switch(code) {
    case 0xE4:
        return "ae";
    case 0xF6:
        return "oe";
    case 0xFC:
        return "ue";
    case 0xC4:
        return "Ae";
    case 0xD6:
        return "Oe";
    case 0xDC:
        return "Ue";
    case 0xDF:
        return "ss";
    default:
        return "?";
    }
}

/* Converts a JSON string to printable ASCII; German umlauts become ae/oe/ue/ss. */
static bool yt_read_string(const char* json, const char* key, char* out, size_t size) {
    if(size == 0U) return false;
    out[0] = '\0';
    const char* value = yt_find_value(json, key);
    if(!value || *value != '"') return false;
    ++value;
    size_t used = 0;
    while(*value && *value != '"' && used + 1U < size) {
        unsigned char byte = (unsigned char)*value;
        if(byte == '\\') {
            ++value;
            char escaped = *value;
            if(!escaped) break;
            if(escaped == 'u') {
                unsigned code = 0;
                int digits = 0;
                while(digits < 4 && value[1]) {
                    char hex = value[1];
                    unsigned nibble;
                    if(hex >= '0' && hex <= '9')
                        nibble = (unsigned)(hex - '0');
                    else if(hex >= 'a' && hex <= 'f')
                        nibble = (unsigned)(hex - 'a' + 10);
                    else if(hex >= 'A' && hex <= 'F')
                        nibble = (unsigned)(hex - 'A' + 10);
                    else
                        break;
                    code = code * 16U + nibble;
                    ++digits;
                    ++value;
                }
                if(code >= 32U && code < 127U) {
                    char single[2] = {(char)code, '\0'};
                    yt_append(out, size, &used, single);
                } else {
                    yt_append(out, size, &used, yt_latin1_ascii(code));
                }
            } else {
                char single[2] = {
                    escaped == 'n' || escaped == 't' || escaped == 'r' ? ' ' : escaped, '\0'};
                yt_append(out, size, &used, single);
            }
            ++value;
        } else if(byte == 0xC3 && value[1]) {
            unsigned char next = (unsigned char)value[1];
            yt_append(out, size, &used, yt_latin1_ascii(0xC0U | (next & 0x3FU)));
            value += 2;
        } else if(byte >= 0xC0) {
            yt_append(out, size, &used, "?");
            ++value;
            while(((unsigned char)*value & 0xC0U) == 0x80U)
                ++value;
        } else if(byte >= 0x80 || byte < 32) {
            ++value; /* Stray continuation or control byte. */
        } else {
            out[used++] = (char)byte;
            out[used] = '\0';
            ++value;
        }
    }
    return true;
}

void yt_parse_response(const char* json, uint16_t http_status, YtStats* out) {
    memset(out, 0, sizeof(*out));
    if(http_status != 200U) {
        out->result = YtParseHttpError;
        char message[YT_ERROR_SIZE - 16U];
        if(yt_read_string(json, "message", message, sizeof(message)) && message[0]) {
            snprintf(out->error, sizeof(out->error), "HTTP %u: %s", http_status, message);
        } else {
            snprintf(out->error, sizeof(out->error), "HTTP %u", http_status);
        }
        return;
    }
    if(!strstr(json, "\"items\"")) {
        out->result = YtParseNotFound;
        return;
    }
    yt_read_string(json, "title", out->title, sizeof(out->title));
    yt_read_number(json, "viewCount", &out->views);
    yt_read_number(json, "videoCount", &out->videos);
    const char* hidden = yt_find_value(json, "hiddenSubscriberCount");
    out->hidden = hidden && strncmp(hidden, "true", 4) == 0;
    if(yt_read_number(json, "subscriberCount", &out->subscribers) || out->hidden) {
        out->ok = true;
        out->result = YtParseOk;
    } else {
        out->result = YtParseNoCount;
    }
}

void yt_format_grouped(uint64_t value, char separator, char* out, size_t size) {
    char digits[24];
    snprintf(digits, sizeof(digits), "%llu", (unsigned long long)value);
    size_t length = strlen(digits);
    size_t used = 0;
    for(size_t i = 0; i < length && used + 1U < size; ++i) {
        if(i > 0 && (length - i) % 3U == 0U) {
            if(used + 2U >= size) break;
            out[used++] = separator;
        }
        out[used++] = digits[i];
    }
    if(size) out[used] = '\0';
}

static bool yt_safe_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '.';
}

static bool yt_all_safe(const char* text) {
    if(!*text) return false;
    for(; *text; ++text)
        if(!yt_safe_char(*text)) return false;
    return true;
}

bool yt_channel_query(const char* channel, char* out, size_t size) {
    if(!channel || !*channel) return false;
    int written;
    if(strlen(channel) == 24U && strncmp(channel, "UC", 2) == 0 && yt_all_safe(channel)) {
        written = snprintf(out, size, "id=%s", channel);
    } else {
        const char* handle = channel[0] == '@' ? channel + 1 : channel;
        if(strlen(handle) > 40U || !yt_all_safe(handle)) return false;
        written = snprintf(out, size, "forHandle=%%40%s", handle);
    }
    return written > 0 && (size_t)written < size;
}

bool yt_api_key_valid(const char* key) {
    if(!key || strlen(key) < 20U || strlen(key) > 60U) return false;
    for(; *key; ++key)
        if(!yt_safe_char(*key) || *key == '.') return false;
    return true;
}
