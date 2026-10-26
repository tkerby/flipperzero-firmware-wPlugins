// The app functions / objects the plugins (battle, menu, ui) may call. Every entry is checked against the real declaration by the
// cast in API_METHOD (a signature that drifts from the header fails to compile). After adding a call in a plugin's sources
// (scene_battle.c / battle_fx.c / battle_gfx.c / battle_run.c / blit_fx.c / hud_gfx.c, scene_party.c / scene_bag.c / scene_shop.c / scene_box.c / scene_card.c / box.c / names.c, scene_title.c / scene_lang.c /
// scene_name.c / scene_options.c / scene_mon.c / scene_save.c) run
// `tools/check_plugin_api.py`: it lists the plugins' undefined symbols that neither the firmware nor this table provide (the
// build prints the same list as a warning). All plugins share the table; an entry only one of them uses is normal.
// Keep the table sorted by area only for reading; the macro sorts it by hash at compile time.
static constexpr auto app_api_table = sort(create_array_t<sym_entry>(
    // battle core (data half; the turn engine is part of the plugin)
    API_METHOD(bc_party_clear, void, (Party*)),
    API_METHOD(bc_mon_create, void, (const BcData*, Mon*, uint8_t, uint8_t, uint32_t*)),
    API_METHOD(bc_mon_recalc, void, (const BcData*, Mon*)),
    API_METHOD(bc_trainer_party, bool, (const BcData*, uint8_t, uint8_t, Party*, uint32_t*)),
    API_METHOD(bc_learn_move, uint8_t, (const BcData*, Mon*, uint8_t, uint8_t*)),
    API_METHOD(
        bc_move_set,
        void,
        (const BcData*, Mon*, uint8_t, uint8_t)), // (the UI plugin's evolution learns with it, M5-B)
    API_METHOD(
        bc_species,
        const SpeciesDef*,
        (const BcData*, uint8_t)), // (post-M5: the records are read on demand, contract 27)
    API_METHOD(bc_move, const MoveDef*, (const BcData*, uint8_t)),
    API_METHOD(bc_trainer, const TrainerDef*, (const BcData*, uint8_t)),
    API_METHOD(bc_exp_gain, uint32_t, (uint8_t, uint8_t, bool)),
    API_METHOD(bc_exp_for_level, uint32_t, (uint8_t, uint8_t)),
    API_METHOD(bc_pp_max, uint8_t, (uint8_t, uint8_t)), // contract 77 (PP Plus)
    API_METHOD(bc_rng_next, uint32_t, (uint32_t*)),
    API_METHOD(bc_rng_seed, void, (uint32_t*, uint32_t)),
    API_METHOD(bc_rand8, uint8_t, (uint32_t*)),
    API_METHOD(bc_rand_range, uint32_t, (uint32_t*, uint32_t, uint32_t)),
    // party / game state
    API_METHOD(party_data, const BcData*, (void)),
    API_METHOD(party_player, Party*, (void)),
    API_METHOD(party_rng, uint32_t*, (void)),
    API_METHOD(party_starter, uint8_t, (void)),
    API_METHOD(
        party_give,
        int,
        (uint8_t, uint8_t)), // (the casino plugin's prize counter, contract 37)
    // names (task K): nicknames for the battle and the party screen, the keyboard's result
    API_METHOD(party_nick, const tchar*, (uint8_t)),
    API_METHOD(party_mon_name, int, (uint8_t, tchar*, int)),
    API_METHOD(party_state, PartyState*, (void)),
    API_METHOD(party_nick_cache, NickRow*, (void)),
    API_METHOD(text_set_names, void, (const tchar*, const tchar*)),
    API_METHOD(file_size, int32_t, (const char*)),
    API_METHOD(font_charset_id, uint32_t, (void)),
    // bag and Registro (bag.c, contract 14)
    API_METHOD(bag_kinds, uint8_t, (void)),
    API_METHOD(bag_get, bool, (uint8_t, uint8_t*, uint8_t*)),
    API_METHOD(bag_take, bool, (uint8_t)),
    // the shop screen (menu plugin, contract 17): bag + money
    API_METHOD(bag_count, uint8_t, (uint8_t)),
    API_METHOD(bag_add, uint8_t, (uint8_t, uint8_t)),
    API_METHOD(money_get, uint32_t, (void)),
    API_METHOD(money_add, uint32_t, (uint32_t)),
    API_METHOD(money_take, bool, (uint32_t)),
    API_METHOD(text_set_var, void, (uint8_t, const tchar*)),
    API_METHOD(reg_set_seen, void, (uint8_t)),
    API_METHOD(reg_set_caught, void, (uint8_t)),
    // M5-C (contract 25): the pending capture (the battle plugin parks it, the UI plugin's nickname step names it, the PC code stores it)
    API_VARIABLE(
        g_pending,
        PartyPending), // M5-C (contract 25): the pending capture (the battle plugin parks it, the UI plugin names it, the PC code stores it); accessors are inline in party.h
    // debug overlay
    API_METHOD(scene_debug_overlay_update, void, (SceneCtx*)),
    API_METHOD(scene_debug_overlay_draw, void, (SceneCtx*)),
    // the battle's end: the app unloads the plugin, restores the map's sprite bank and fades back in (battle_open.c)
    API_METHOD(battle_scene_closed, void, (SceneCtx*)),
    API_VARIABLE(g_battle_leveled, uint8_t),
    API_VARIABLE(
        g_link_foe,
        LinkFoe*), // IR LINK (contract 45): the friend's team: the link plugin makes it, the battle plugin plays against it
    // scenes
    API_METHOD(scene_push, bool, (SceneCtx*, const SceneOps*, void*)),
    API_METHOD(scene_pop, void, (SceneCtx*)),
    API_METHOD(scene_clear, void, (SceneCtx*)),
    // drawing
    API_METHOD(blit_masked, void, (LvlBuf, const Blit*, int, int)),
    API_METHOD(hero_view, void, (uint8_t, uint8_t, Blit*)),
    API_METHOD(render_cache_release, void, (void)),
    API_METHOD(monpic_free, void, (MonPic*)),
    API_METHOD(monpic_load_kind, bool, (MonPic*, uint8_t, uint8_t)),
    API_METHOD(ov_fill, void, (int, int, int, int, uint8_t)),
    API_METHOD(ov_frame, void, (int, int, int, int, uint8_t)),
    API_METHOD(ov_bitmap, void, (int, int, int, int, const uint8_t*, int, uint8_t)),
    // fonts and text
    API_METHOD(font_pitch, int, (void)),
    API_METHOD(font_baseline, int, (void)),
    API_METHOD(font_cell_w, int, (void)),
    API_METHOD(font_adv, int, (tchar)),
    API_METHOD(font_text_width, int, (const tchar*)),
    API_METHOD(font_draw_glyph, void, (tchar, int, int, uint8_t)),
    API_METHOD(font_draw_text, void, (const tchar*, int, int)),
    API_METHOD(text_language, uint8_t, (void)),
    API_METHOD(text_load, int, (uint16_t, tchar*, int)),
    API_METHOD(text_set_var_id, bool, (uint8_t, uint16_t)),
    API_METHOD(text_set_var_num, void, (uint8_t, int)),
    API_METHOD(text_set_var_rival, void, (uint8_t)),
    API_METHOD(text_clear_vars, void, (void)),
    API_METHOD(text_glyph_ms, uint16_t, (void)),
    API_METHOD(text_line_start, int, (const tchar*, int, int)),
    API_METHOD(text_line_glyphs, int, (const tchar*, int)),
    API_METHOD(text_line_px, int, (const tchar*, int)),
    API_METHOD(text_next_page, int, (const tchar*, int)),
    API_METHOD(text_draw_line, int, (const tchar*, int, int, int, int)),
    API_METHOD(text_prefetch_page, void, (const tchar*, int)),
    API_METHOD(text_prefetch_many, void, (const tchar* const*, int)),
    // asset files (battle effects: fx.bin frames streamed by battle_fx.c)
    API_METHOD(read_at, bool, (const char*, uint32_t, void*, uint32_t)),
    // heap
    API_METHOD(heap_can_alloc, bool, (uint32_t)),
#ifdef NUCLEO_DEBUG // only the UI plugin's title heap label (dev build) calls these four
    API_METHOD(heap_stats_now, void, (HeapStats*)),
    API_METHOD(heap_format, int, (const HeapStats*, char*, size_t)),
#endif
    // UI plugin (contract 11.5): scenes of the title / pickers / options / popup / save screen call the app's flow functions
    API_METHOD(scene_dialog_open, bool, (SceneCtx*, uint16_t, int8_t)),
    API_METHOD(scene_ask_open, bool, (SceneCtx*, uint16_t, uint8_t*)),
    API_METHOD(scene_fade_new_game, bool, (SceneCtx*)),
    API_METHOD(scene_fade_continue, bool, (SceneCtx*)),
    API_METHOD(scene_fade_to_title, bool, (SceneCtx*)),
    API_METHOD(world_go_respawn, void, (World*)), // (the bag's Exit Wisp, C2-S1)
    API_METHOD(scene_menu_reload, void, (SceneCtx*)),
    API_METHOD(scene_menu_set_hidden, void, (bool)),
    API_METHOD(world_draw_frozen, void, (SceneCtx*)),
    API_METHOD(
        world_bike_toggle,
        bool,
        (void)), // (the bag's Usar on the bicycle, C4-A contract 35.4)
    // C5 (contract 42): the world features run in the PC plugin (scene_card.c world_feat): the surf / Empuje prompts and the fishing cast act on THE world
    API_VARIABLE(g_w, World*),
    API_METHOD(
        world_can_enter,
        bool,
        (const World*, int, int)), // (the PC plugin's roamer placement, contract 55)
    API_VARIABLE(
        world_party_ready_hook,
        uint16_t (*)(
            void)), // (same: the first creature able to fight, level | species << 8, 0 = none)
    API_METHOD(world_start_step, bool, (World*, uint8_t, bool)),
    API_METHOD(world_start_wild, bool, (World*, uint8_t, uint8_t, int)),
    API_METHOD(
        scene_fade_warp,
        bool,
        (SceneCtx*,
         uint8_t,
         uint8_t,
         uint8_t,
         uint8_t)), // (the casino plugin's Reserva end scene: the fade home, contract 43.2)
    API_METHOD(save_log_why, void, (int)),
    API_METHOD(settings_crc32, uint32_t, (const uint8_t*, uint32_t)),
    API_METHOD(save_time_secs, uint32_t, (void)),
    API_METHOD(settings_current_lang_index, int, (void)),
    API_VARIABLE(
        g_settings,
        Settings), // (settings_io.c, UI plugin: settings_store_language writes the language into it)
    API_METHOD(reg_caught_count, uint8_t, (void)),
    API_METHOD(monpic_load, bool, (MonPic*, uint8_t, bool)),
    API_METHOD(blit_auto, void, (LvlBuf, const Blit*, int, int)),
    API_METHOD(tb_geom, const TbGeom*, (void)),
    API_METHOD(text_prefetch, void, (const tchar*)),
    API_METHOD(text_line_len, int, (const tchar*, int)),
    API_METHOD(text_set_language, bool, (uint8_t)),
    API_METHOD(text_set_speed, void, (uint8_t)), // (the Options row "Texto", contract 75)
#ifdef NUCLEO_DEBUG
    API_METHOD(sysfont_draw_str, void, (const uint8_t*, int, int)),
#endif
    // sound (contract 28): the plugins play the level-up / capture / victory / hit / evolution / menu sounds
    API_METHOD(sound_play, void, (uint16_t))));
