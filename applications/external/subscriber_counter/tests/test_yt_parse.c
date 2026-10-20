#include "../yt_parse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    YtStats s;
    char buf[64];
    yt_parse_response(
        "{\n  \"items\": [\n    {\n      \"snippet\": {\n        \"title\": \"M\\u00fcller \\\"TV\\\" Gr\xc3\xbc\xc3\x9f"
        "e \xf0\x9f\x8e\xae\"\n      },\n"
        "      \"statistics\": {\n        \"viewCount\": \"1234567\",\n        \"subscriberCount\": \"12300\",\n"
        "        \"hiddenSubscriberCount\": false,\n        \"videoCount\": \"42\"\n      }\n    }\n  ]\n}\n",
        200,
        &s);
    assert(s.ok && !s.hidden);
    assert(s.subscribers == 12300 && s.views == 1234567 && s.videos == 42);
    printf("title: [%s]\n", s.title);
    assert(strcmp(s.title, "Mueller \"TV\" Gruesse ?") == 0);

    yt_parse_response(
        "{\"items\":[{\"snippet\":{\"title\":\"X\"},\"statistics\":{\"viewCount\":\"5\",\"hiddenSubscriberCount\":true,\"videoCount\":\"1\"}}]}",
        200,
        &s);
    assert(s.ok && s.hidden);

    yt_parse_response("{}", 200, &s);
    assert(!s.ok && s.result == YtParseNotFound);

    yt_parse_response(
        "{\n \"error\": {\n  \"code\": 400,\n  \"message\": \"API key not valid. Please pass a valid API key.\",\n  \"errors\": []}}",
        400,
        &s);
    assert(!s.ok && s.result == YtParseHttpError);
    printf("err: %s\n", s.error);
    assert(strstr(s.error, "API key not valid"));

    yt_parse_response("not json", 503, &s);
    assert(s.result == YtParseHttpError && strcmp(s.error, "HTTP 503") == 0);
    yt_parse_response("{\"items\":[{\"statistics\":{\"viewCount\":\"5\"}}]}", 200, &s);
    assert(!s.ok && s.result == YtParseNoCount);
    yt_parse_response(
        "{\"items\":[{\"statistics\":{\"subscriberCount\": \"99999999999999999999999\"}}]}",
        200,
        &s);
    assert(!s.ok);

    yt_format_grouped(0, ',', buf, sizeof buf);
    assert(!strcmp(buf, "0"));
    yt_format_grouped(999, ',', buf, sizeof buf);
    assert(!strcmp(buf, "999"));
    yt_format_grouped(1000, ',', buf, sizeof buf);
    assert(!strcmp(buf, "1,000"));
    yt_format_grouped(1234567, ',', buf, sizeof buf);
    assert(!strcmp(buf, "1,234,567"));
    yt_format_grouped(1234567, '.', buf, sizeof buf);
    assert(!strcmp(buf, "1.234.567"));
    yt_format_grouped(1234567, ',', buf, 6);
    assert(!strcmp(buf, "1,234"));

    assert(
        yt_channel_query("UC_x5XG1OV2P6uZZ5FSM9Ttw", buf, sizeof buf) &&
        !strcmp(buf, "id=UC_x5XG1OV2P6uZZ5FSM9Ttw"));
    assert(yt_channel_query("@MrBeast", buf, sizeof buf) && !strcmp(buf, "forHandle=%40MrBeast"));
    assert(
        yt_channel_query("my.channel-1", buf, sizeof buf) &&
        !strcmp(buf, "forHandle=%40my.channel-1"));
    assert(!yt_channel_query("@bad&key=x", buf, sizeof buf));
    assert(!yt_channel_query("", buf, sizeof buf));
    assert(yt_api_key_valid("AIzaSyA-abcdefghijklmnopqrstuvwxyz_0123"));
    assert(!yt_api_key_valid("short"));
    assert(!yt_api_key_valid("AIzaSyA-abcdefghijklmn&opqrstuvwxyz0123"));
    puts("ALL OK");
    return 0;
}
