#include "flipper.h"
#include "app_state.h"
#include "arduino.h"

int32_t fcom_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "fcom app launched");
    App* app = app_alloc();
    app->state = app_state_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FcomMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
