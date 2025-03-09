#ifndef _ADVANCED_SUBMENU_UI_VIEW_CLASS_
#define _ADVANCED_SUBMENU_UI_VIEW_CLASS_

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>

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
    const char* noElementsCapton = NULL;

    const char* leftButtonCaption = NULL;
    const char* ceneterButtonCaption = NULL;
    const char* rightButtonCaption = NULL;

    size_t selected = 0;
    size_t elements = 0;

    function<const char*(size_t)> getElementName;

    void draw(Canvas* canvas) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);

        if(leftButtonCaption != NULL) elements_button_left(canvas, leftButtonCaption);
        if(ceneterButtonCaption != NULL) elements_button_center(canvas, ceneterButtonCaption);
        if(rightButtonCaption != NULL) elements_button_right(canvas, rightButtonCaption);

        if(elements == 0 && noElementsCapton != NULL) {
            int wCenter = canvas_width(canvas) / 2;
            int hCenter = canvas_height(canvas) / 2;
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, wCenter, hCenter, AlignCenter, AlignCenter, noElementsCapton);
            canvas_set_font(canvas, FontSecondary);
        }
    }

public:
    AdvancedSubMenuUiView() {
        view = view_alloc();
        view_set_context(view, this);

        view_set_draw_callback(view, drawCallback);
        view_set_input_callback(view, inputCallback);
        view_set_enter_callback(view, enterCallback);
        view_set_exit_callback(view, exitCallback);

        view_allocate_model(view, ViewModelTypeLockFree, sizeof(AdvancedSubMenuUiView*));
        with_view_model_cpp(view, AdvSubMenuViewModel*, model, model->uiVIew = this;, false);
    }

    AdvancedSubMenuUiView(const char* noElementsCapton) : AdvancedSubMenuUiView() {
        this->noElementsCapton = noElementsCapton;
    }

    void SetLeftButton(const char* caption) {
        leftButtonCaption = caption;
    }

    void SetCenterButton(const char* caption) {
        ceneterButtonCaption = caption;
    }

    void SetRightButton(const char* caption) {
        rightButtonCaption = caption;
    }

    void AddElement() {
        if(elements == 0 || selected == elements - 1) {
            selected = elements;
        }
        elements++;
    }

    bool input(InputEvent* event) {
        UNUSED(event);
        return false;
    }

    View* GetNativeView() {
        return view;
    }

    ~AdvancedSubMenuUiView() {
        if(view != NULL) {
            OnDestory();
            view_free_model(view);
            view_free(view);
            view = NULL;
        }
    }

private:
    static void drawCallback(Canvas* canvas, void* model) {
        AdvancedSubMenuUiView* uiView = (AdvancedSubMenuUiView*)((AdvSubMenuViewModel*)model)->uiVIew;
        uiView->draw(canvas);
    }

    static bool inputCallback(InputEvent* event, void* context) {
        AdvancedSubMenuUiView* uiView = (AdvancedSubMenuUiView*)context;
        return uiView->input(event);
    }

    static void enterCallback(void* context) {
        UNUSED(context);
    }

    static void exitCallback(void* context) {
        UNUSED(context);
    }
};

#endif //_ADVANCED_SUBMENU_UI_VIEW_CLASS_
