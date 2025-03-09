#ifndef _ADVANCED_SUBMENU_UI_VIEW_CLASS_
#define _ADVANCED_SUBMENU_UI_VIEW_CLASS_

#include <furi.h>
#include <gui/gui.h>

#include "UiView.cpp"
#include "lib/HandlerContext.cpp"

#undef LOG_TAG
#define LOG_TAG "UI_ADV_SUBMENU"

using namespace std;

// Inspired by https://github.com/flipperdevices/flipperzero-firmware/blob/dev/applications/main/subghz/views/receiver.c

struct AdvSubMenuViewModel {
    void* uiVIew;
};

class AdvancedSubMenuUiView : public UiView {
private:
    View* view = NULL;

    uint8_t rssiLevel = 0;
    uint32_t elementCount = 0;

public:
    AdvancedSubMenuUiView() {
        view = view_alloc();
        view_set_context(view, this);

        view_set_draw_callback(view, drawCallback);
        view_set_input_callback(view, inputCallback);
        view_set_enter_callback(view, enterCallback);
        view_set_exit_callback(view, exitCallback);

        view_allocate_model(view, ViewModelTypeLockFree, sizeof(AdvancedSubMenuUiView*));
        with_view_model_cpp(view, AdvSubMenuViewModel, model, { model.uiVIew = this; }, false);
    }

    View* GetNativeView() {
        return view;
    }

    ~AdvancedSubMenuUiView() {
        if(view != NULL) {
            OnDestory();
            view_free(view);
            view = NULL;
        }
    }

private:
    static void drawCallback(Canvas* canvas, void* model) {
        AdvancedSubMenuUiView* uiView = ((AdvSubMenuViewModel*)model)->uiVIew;
    }

    static bool inputCallback(InputEvent* event, void* context) {
    }

    static void enterCallback(void* context) {
    }

    static void exitCallback(void* context) {
    }
};

#endif //_ADVANCED_SUBMENU_UI_VIEW_CLASS_
