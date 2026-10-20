#pragma once

#include <stdint.h>

/* Inter Display Bold for the hero number (BIG, MID when it would not fit),
 * Inter SemiBold for the header (TITLE) and Inter Medium for labels (LABEL).
 * *_BASELINE are y positions; LABEL_CAP is the label cap/digit height. */
extern const uint8_t yt_inter_big[];
extern const uint8_t yt_inter_mid[];
extern const uint8_t yt_inter_title[];
extern const uint8_t yt_inter_label[];

#define YT_FONT_BIG       yt_inter_big
#define YT_FONT_MID       yt_inter_mid
#define YT_FONT_TITLE     yt_inter_title
#define YT_FONT_LABEL     yt_inter_label
#define YT_TITLE_BASELINE 10
#define YT_LABEL_BASELINE 49
#define YT_LABEL_CAP      8
