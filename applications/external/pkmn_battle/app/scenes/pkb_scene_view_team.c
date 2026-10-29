/* Shows the team Start Battle will use, one mon per page with its sprite;
 * builds it if there is none yet. Left/Right page; for random teams OK
 * draws a new one (and every battle after the first gets a new one). */
#include "../pkb_app.h"
#include "../storage_glue.h"
#include "pkb_scene.h"

/* Reads the species' sprite from all_sprites.fxbm; false if missing or bad. */
static bool view_team_load_sprite(PkbApp* app, uint8_t species, uint8_t* out) {
    PkbSdBlob blob;
    PkbBlobReader r;
    uint32_t head[3]; /* record size, width, height */
    bool ok = false;

    if(species < 1 || species > PKB_NUM_SPECIES) return false;
    if(!pkb_sd_blob_open(&blob, app->storage, PKB_SPRITES_PATH, &r)) return false;
    uint32_t off = (uint32_t)(species - 1) * PKB_SPRITE_RECORD;
    if(r.read(r.ctx, off, head, sizeof(head)) && head[0] == PKB_SPRITE_RECORD - 4 &&
       head[1] == PKB_SPRITE_SIZE && head[2] == PKB_SPRITE_SIZE) {
        ok = r.read(r.ctx, off + sizeof(head), out, PKB_SPRITE_BYTES);
    }
    pkb_sd_blob_close(&blob);
    return ok;
}

static void view_team_show(PkbApp* app, uint8_t page) {
    PkbTeamViewModel* m = &app->team_page;
    const PkbTeamMon* mon = &app->team.mon[page];

    m->page = page;
    m->count = app->team.count;
    pkb_species_name(mon->species, m->name, sizeof(m->name));
    m->level = mon->level;
    m->max_hp = mon->max_hp;
    pkb_item_name(mon->item, m->item, sizeof(m->item));
    m->can_reroll = app->settings.kind == PkbOpponentRandom;
    for(int k = 0; k < PKB_NUM_MOVES; k++) {
        pkb_move_name(mon->moves[k], m->moves[k], sizeof(m->moves[k]));
    }
    m->has_sprite = view_team_load_sprite(app, mon->species, m->sprite);
    pkb_team_view_update(app->team_view, m);
}

void pkb_scene_view_team_on_enter(void* context) {
    PkbApp* app = context;
    if(app->have_team || pkb_app_new_team(app)) {
        view_team_show(app, 0);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, PKB_EVENT_ERROR);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewTeam);
}

bool pkb_scene_view_team_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    uint8_t page = app->team_page.page, count = app->team.count;
    switch(event.event) {
    case PKB_TEAM_VIEW_EVENT_PREV:
        view_team_show(app, page ? page - 1 : count - 1);
        return true;
    case PKB_TEAM_VIEW_EVENT_NEXT:
        view_team_show(app, page + 1 < count ? page + 1 : 0);
        return true;
    case PKB_TEAM_VIEW_EVENT_REROLL:
        if(app->settings.kind != PkbOpponentRandom) return true;
        if(pkb_app_new_team(app)) {
            view_team_show(app, 0);
        } else {
            scene_manager_next_scene(app->scene_manager, PkbSceneError);
        }
        return true;
    case PKB_EVENT_ERROR:
        scene_manager_next_scene(app->scene_manager, PkbSceneError);
        return true;
    }
    return false;
}

void pkb_scene_view_team_on_exit(void* context) {
    UNUSED(context);
}
