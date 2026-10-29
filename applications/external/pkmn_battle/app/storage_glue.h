#pragma once

#include <storage/storage.h>
#include <pkb/team.h>

/* files/movepools.bin as deployed by fap_file_assets */
#define PKB_MOVEPOOLS_PATH APP_ASSETS_PATH("movepools.bin")
/* 56x56 species sprites from the Pokemon Trade Tool (MIT), one record per
 * species in dex order: u32 400, u32 width, u32 height, 392 bytes XBM */
#define PKB_SPRITES_PATH   APP_ASSETS_PATH("all_sprites.fxbm")
#define PKB_SPRITE_RECORD  404

typedef struct {
    File* file;
} PkbSdBlob;

/* Opens `path` read-only and fills `reader`. Returns false if missing. */
bool pkb_sd_blob_open(PkbSdBlob* blob, Storage* storage, const char* path, PkbBlobReader* reader);
void pkb_sd_blob_close(PkbSdBlob* blob);
