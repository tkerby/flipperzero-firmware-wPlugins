#include <datetime.h>
#include <stdbool.h>
#include <furi.h>

#include "suica_structs_i.h"


#define SUICA_RAILWAY_NUM 32 // Don't count Unknown

#define SUICA_RAILWAY_UNKNOWN_NAME "Unknown"
// Railway

static const Railway RailwaysList[] = {
    // Japan Railway East JREast
    {0x01, {0, 0}, "Keihin Tohoku", 14, SuicaJREast, "JK", 0},
    {0x01, {0, 0}, "East Tokaido", 21, SuicaJREast, "JT", 0},
    {0x02, {0, 0}, "Keihin Tohoku", 22, SuicaJREast, "JK", 0},
    {0x02, {0, 0}, "Utsunomiya", 33, SuicaJREast, "JU", 0},
    {0x02, {0, 0}, "Mobile Suica", 1, SuicaMobile, "", 0},
    {0x03, {0, 0}, "Chuo Rapid", 32, SuicaJREast, "JC", 0},
    {0x03, {0, 0}, "Chuo Main", 29, SuicaJREast, "CO", 0},
    {0x03, {0, 0}, "Chuo West", 37, SuicaJRCentral, "CF", 0},
    {0x03, {0, 0}, "Chuo-Sobu", 18, SuicaJREast, "JB", 0},
    {0x05, {0, 0}, "Joban", 71, SuicaJREast, "JJ", 0},
    {0x05, {0, 0}, "Joban Local", 14, SuicaJREast, "JL", 0},
    {0x1D, {0, 0}, "Negishi", 10, SuicaJREast, "JK", 0},

    // Japan Railway Central JRCentral
    {0x01, {0, 0}, "Central Tokaido", 83, SuicaJRCentral, "CA", 0},

    // Tokyo Waterfront Area Rapid Transit TWR
    {0x82, {0, 0}, "Rinkai", 8, SuicaTWR, "R", &I_Suica_RinkaiR},
    {0x82, {0, 0}, "Yurikamome", 16, SuicaYurikamome, "U", &I_Suica_YurikamomeU},

    // Tokyo Monorail
    {0xFA, {0, 0}, "Tokyo Monorail", 11, SuicaTokyoMonorail, "MO", 0},

    // Keikyu
    {0xD5, {0, 0}, "Keikyu Main", 50, SuicaKeikyu, "KK", &I_Suica_KeikyuKK},
    {0xD6, {0, 0}, "Keikyu Airport", 6, SuicaKeikyu, "KK", &I_Suica_KeikyuKK},

    // Tokyo Metro
    {0xE3, {0, 0}, "Ginza", 19, SuicaTokyoMetro, "G", &I_Suica_GinzaG},
    {0xE3, {1, 0}, "Chiyoda", 20, SuicaTokyoMetro, "C", &I_Suica_ChiyodaC},
    {0xE3, {1, 1}, "Yurakucho", 24, SuicaTokyoMetro, "Y", &I_Suica_YurakuchoY},
    {0xE4, {1, 0}, "Hibiya", 21, SuicaTokyoMetro, "H", &I_Suica_HibiyaH},
    {0xE4, {2, 1}, "Tozai", 23, SuicaTokyoMetro, "T", &I_Suica_TozaiT},
    {0xE5, {0, 1}, "Marunouchi", 25, SuicaTokyoMetro, "M", &I_Suica_MarunouchiM},
    {0xE5, {-5, 1}, "M Honancho", 4, SuicaTokyoMetro, "Mb", &I_Suica_MarunouchiHonanchoMb},
    {0xE6, {2, 1}, "Hanzomon", 14, SuicaTokyoMetro, "Z", &I_Suica_HanzomonZ},
    {0xE7, {0, 1}, "Namboku", 19, SuicaTokyoMetro, "N", &I_Suica_NambokuN},
    
    // Toei
    {0xEF, {0, 0}, "Asakusa", 20, SuicaToei, "A", &I_Suica_AsakusaA},
    {0xF0, {4, 0}, "Mita", 27, SuicaToei, "I", &I_Suica_MitaI},
    {0xF1, {2, 0}, "Shinjuku", 21, SuicaToei, "S", &I_Suica_ShinjukuS},
    {0xF2, {3, 0}, "Oedo", 26, SuicaToei, "E", &I_Suica_OedoE},
    {0xF3, {3, 0}, "Oedo", 14, SuicaToei, "E", &I_Suica_OedoE},
    // Unknown
    {0x00, {0, 0}, SUICA_RAILWAY_UNKNOWN_NAME, 1, SuicaRailwayTypeMax, "??", &I_Suica_QuestionMarkBig}
};
