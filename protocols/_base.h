#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Forward decls for things we only need by pointer here.
 * (Icons are defined elsewhere, e.g. assets/icons.h)
 */
typedef struct Icon Icon;

/* ===== Payload support ===== */

typedef enum {
    PayloadModeRandom = 0,
    PayloadModeValue  = 1,
    PayloadModeBruteforce = 2,
} PayloadMode;

typedef struct {
    uint32_t value;   // current/bruteforced value
    uint8_t  size;    // number of bytes/bits that are significant
    uint32_t counter; // iteration counter for bruteforce mode
} Bruteforce;

/*
 * Per-protocol config structs.
 * These are intentionally lightweight and live here to avoid circular includes.
 * If you have richer versions in the working ZIP, you can replace these with
 * the exact field layouts — callers already use these names.
 */

/* --- Easy Setup (Samsung buds/watch) --- */
typedef struct {
    struct {
        struct { uint32_t model; } buds;   // used as cfg->data.buds.model (3 digits / up to 24 bits)
        struct { uint8_t  model; } watch;  // used as cfg->data.watch.model (1 digit)
    } data;
} EasysetupCfg;

/* --- Fast Pair (Android) --- */
typedef struct {
    uint32_t model; // used as cfg->model (3 digits)
} FastpairCfg;

/* --- Apple Continuity (Proximity Pairing / Nearby Action) --- */
typedef struct {
    struct {
        struct {
            uint16_t model; // used as cfg->data.proximity_pair.model (2 digits)
            uint8_t  color; // used as cfg->data.proximity_pair.color (1 digit)
        } proximity_pair;
        struct {
            uint8_t action; // used as cfg->data.nearby_action.action (1 digit)
        } nearby_action;
    } data;
} ContinuityCfg;

/* You can add other protocol cfg shells here if needed.
   They can stay empty until something actually uses them. */
typedef struct { /* placeholder */ } LovespouseCfg;
typedef struct { /* placeholder */ } NamefloodCfg;
typedef struct { /* placeholder */ } SwiftpairCfg;
typedef struct { uint16_t id; } MagicbandCfg;

/* The payload shared by all protocols */
typedef struct {
    PayloadMode mode;
    Bruteforce  bruteforce;
    union {
        ContinuityCfg continuity;
        EasysetupCfg  easysetup;
        FastpairCfg   fastpair;
        LovespouseCfg lovespouse;
        NamefloodCfg  nameflood;
        SwiftpairCfg  swiftpair;
        MagicbandCfg  magicband;
    } cfg;
} Payload;

/* ===== Protocol V-table =====
   - extra_config takes a generic context pointer; scene/app code will cast it.
*/
typedef struct Protocol {
    const Icon* (*icon)(void); // If your code uses &I_android/&I_apple directly, change this to `const Icon* icon;`
    const char* (*get_name)(const Payload* payload);
    void        (*make_packet)(uint8_t* out_size, uint8_t** out_packet, Payload* payload);
    void        (*extra_config)(void* ctx);
    uint8_t     (*config_count)(const Payload* payload);
} Protocol;