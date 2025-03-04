#ifndef _UI_MANAGER_CLASS_
#define _UI_MANAGER_CLASS_

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <stack>

#include "view/UiView.cpp"

using namespace std;

static void* __ui_manager_instance = NULL;

class UiManager {
private:
    Gui* gui = NULL;
    ViewDispatcher* viewDispatcher = NULL;
    stack<UiView*> viewStack;

    UiManager() {
        __ui_manager_instance = this;
    }

    static uint32_t backCallback(void* context) {
        UNUSED(context);
        GetInstance()->popView();
        return GetInstance()->currentViewId();
    }

    void popView() {
        UiView* currentView = viewStack.top();
        view_dispatcher_remove_view(viewDispatcher, currentViewId());
        delete currentView;
        viewStack.pop();
    }

    uint32_t currentViewId() {
        if(viewStack.empty()) {
            return VIEW_NONE;
        }
        return viewStack.size();
    }

public:
    static UiManager* GetInstance() {
        if(__ui_manager_instance == NULL) {
            __ui_manager_instance = new UiManager();
        }
        return (UiManager*)__ui_manager_instance;
    }

    void InitGui() {
        gui = (Gui*)furi_record_open(RECORD_GUI);
        viewDispatcher = view_dispatcher_alloc();
        view_dispatcher_attach_to_gui(viewDispatcher, gui, ViewDispatcherTypeFullscreen);
    }

    void PushView(UiView* view) {
        viewStack.push(view);

        view_set_previous_callback(view->GetNativeView(), backCallback);
        view_dispatcher_add_view(viewDispatcher, currentViewId(), view->GetNativeView());
        view_dispatcher_switch_to_view(viewDispatcher, currentViewId());
    }

    void RunEventLoop() {
        view_dispatcher_run(viewDispatcher);
    }

    ~UiManager() {
        while(!viewStack.empty()) {
            popView();
        }

        if(viewDispatcher != NULL) {
            view_dispatcher_free(viewDispatcher);
        }

        if(gui != NULL) {
            furi_record_close(RECORD_GUI);
        }
    }
};

#endif //_UI_MANAGER_CLASS_
