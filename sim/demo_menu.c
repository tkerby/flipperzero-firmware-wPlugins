// A tiny app that exists only to prove the ported GUI modules work end to end.
// Build with SIM_ENTRY=demo_menu_app and the scripted driver renders it like any other app.
#include <shim.h>

static int g_picked = -1;

static void picked(void* ctx, uint32_t index) {
    (void)ctx;
    g_picked = (int)index;
    FURI_LOG_I("demo", "menu item %lu picked", (unsigned long)index);
}

int32_t demo_menu_app(void* p) {
    (void)p;
    Menu* menu = menu_alloc();
    if(!menu) return -1;
    menu_set_header(menu, "Demo");
    menu_add_item(menu, "Read", NULL, 0, picked, NULL);
    menu_add_item(menu, "Read RAW", NULL, 1, picked, NULL);
    menu_add_item(menu, "Saved", NULL, 2, picked, NULL);
    menu_add_item(menu, "Add Manually", NULL, 3, picked, NULL);
    menu_add_item(menu, "Frequency Analyzer", NULL, 4, picked, NULL);

    ViewDispatcher* vd = view_dispatcher_alloc();
    if(!vd) return -1;
    view_dispatcher_add_view(vd, 0, menu_get_view(menu));
    view_dispatcher_switch_to_view(vd, 0);
    view_dispatcher_run(vd); // the simulator's driver takes over from here

    FURI_LOG_I("demo", "picked=%d", g_picked);
    menu_free(menu);
    view_dispatcher_free(vd);
    return 0;
}
