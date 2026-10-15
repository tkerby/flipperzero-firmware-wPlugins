#pragma once
#include "../core/nb_backend.h"

typedef enum { MOCK_NONE = 0, MOCK_MFC1K, MOCK_UL, MOCK_ISO4A } MockCard;

extern NbBackend mock_be;
void mock_reset(MockCard kind);
void mock_set_card(MockCard kind);
void mock_set_time(uint32_t ms);
extern int mock_detect_calls, mock_release_calls, mock_apdu_calls, mock_raw_calls;
extern size_t mock_last_raw_bits;
extern bool mock_last_raw_crc_tx, mock_last_raw_crc_rx;
extern uint8_t mock_mfc[64][16];
extern uint8_t mock_ul[16][4];
