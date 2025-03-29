#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PASSPORT_ON_OFF_COUNT 2

#define XP_MODE_COUNT 7

#define XP_MODE_BAR              0
#define XP_MODE_PERCENT          1
#define XP_MODE_INVERTED_PERCENT 2
#define XP_MODE_RETRO_3          3
#define XP_MODE_RETRO_5          4
#define XP_MODE_BAR_PERCENT      5
#define XP_MODE_NONE             6

#define MOOD_SET_COUNT 3

#define MOOD_SET_NONE    0
#define MOOD_SET_REGULAR 1
#define MOOD_SET_420     2

#define PASSPORT_BG_COUNT 15

#define BG_NONE      0
#define BG_ADPOLICE  1
#define BG_CIRCUIT   2
#define BG_DB        3
#define BG_DEDSEC    4
#define BG_STOCK     5
#define BG_FURI      6
#define BG_GITS      7
#define BG_MARIO     8
#define BG_MEDIEVAL  9
#define BG_MEMCHIP   10
#define BG_MOUNTAINS 11
#define BG_MULTI     12
#define BG_SCROLL    13
#define BG_SLUT      14

#define PROFILE_IMG_COUNT 73

#define PIMG_NONE         0
#define PIMG_ADPOLICE     1
#define PIMG_AKKAN        2
#define PIMG_AKKEI        3
#define PIMG_AKTET        4
#define PIMG_ANDROID      5 // (By evillero)
#define PIMG_BLBalalaika  6
#define PIMG_BLBenny      7
#define PIMG_BLDutch      8
#define PIMG_BLRevy       9
#define PIMG_BLRoberta    10
#define PIMG_BLRock       11
#define PIMG_BRIAREOS     12
#define PIMG_DALI         13
#define PIMG_DEDSEC       14
#define PIMG_DEER         15
#define PIMG_DOKKAEBI     16
#define PIMG_DOLPHIN      17
#define PIMG_DOLPHINMOODY 18
#define PIMG_ED209        19
#define PIMG_FALLOUT      20
#define PIMG_FSOCIETY     21
#define PIMG_FSOCIETY2    22
#define PIMG_GITSAOI      23
#define PIMG_GITSARA      24
#define PIMG_GITSBAT      25
#define PIMG_GITSHID      26
#define PIMG_GITSISH      27
#define PIMG_GITSKUS      28
#define PIMG_GITSPRO      29
#define PIMG_GITSSAI      30
#define PIMG_GITSTOG      31
#define PIMG_GOKUSET      32
#define PIMG_GOKUKID      33
#define PIMG_GOKUADULT    34
#define PIMG_GOKUSSJ      35
#define PIMG_GOKUSSJ3     36
#define PIMG_GTAVFRANKLIN 37 // (By evillero)
#define PIMG_GTAVMICHAEL  38 // (By evillero)
#define PIMG_GTAVTREVOR   39 // (By evillero)
#define PIMG_GUYFAWKES    40
#define PIMG_HAUNTER      41
#define PIMG_LAIN         42
#define PIMG_LEEROY       43
#define PIMG_MARIO        44
#define PIMG_MARVIN       45
#define PIMG_MORELEELLOO  46
#define PIMG_NEUROMANCER  47
#define PIMG_O808Benten   48
#define PIMG_O808Gogul    49
#define PIMG_O808Sengoku  50
#define PIMG_PIKASLEEPY   51
#define PIMG_PIRATE       52 // Pirate Profile Pic (By cyberartemio)
#define PIMG_PKMNTR       53
#define PIMG_PSYDUCK      54
#define PIMG_RABBIT       55
#define PIMG_SCARMLA      56
#define PIMG_SCCOBRA      57
#define PIMG_SCCRYBO      58
#define PIMG_SCDOMRO      59
#define PIMG_SCSANDRA     60
#define PIMG_SCTARBEIGE   61
#define PIMG_SHINKAI      62
#define PIMG_SKULL        63
#define PIMG_SLIME        64
#define PIMG_SONIC        65
#define PIMG_SPIDER       66
#define PIMG_TANKGIRL     67
#define PIMG_TOTORO       68
#define PIMG_WAIFU1       69
#define PIMG_WAIFU2       70
#define PIMG_WAIFU3       71
#define PIMG_WRENCH       72

typedef struct {
    uint8_t background;
    uint8_t image;
    bool name;
    uint8_t mood_set;
    bool level;
    bool xp_text;
    uint8_t xp_mode;
    bool multipage;
} PassportSettings;

bool passport_settings_load(PassportSettings* passport_settings);

bool passport_settings_save(PassportSettings* passport_settings);

#ifdef __cplusplus
}
#endif
