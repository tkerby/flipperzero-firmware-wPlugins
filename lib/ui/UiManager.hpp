#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <stack>

#include "view/UiView.hpp"

#undef LOG_TAG
#define LOG_TAG "UI_MGR"

using namespace std;

static void* __ui_manager_instance = NULL;

class UiManager {
private:
    Gui* gui = NULL;
    ViewDispatcher* viewDispatcher = NULL;
    stack<UiView*> viewStack;

    UiManager() {
    }

    static uint32_t backCallback(void* context) {
        UNUSED(context);
        FURI_LOG_I(LOG_TAG, "Back callback called");

        UiManager* uiManager = GetInstance();
        uiManager->popView(false);

        return uiManager->currentViewId();
    }

    void popView(bool preserveView) {
        UiView* currentView = viewStack.top();
        currentView->SetOnTop(false);
        view_dispatcher_remove_view(viewDispatcher, currentViewId());
        viewStack.pop();

        if(!preserveView) {
            delete currentView;
        }

        if(!viewStack.empty()) {
            UiView* viewReturningTo = viewStack.top();
            viewReturningTo->SetOnTop(true);
            viewReturningTo->OnReturn();
        }

        FURI_LOG_I(LOG_TAG, "ViewStack popped, size: %d", viewStack.size());
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
        FURI_LOG_I(LOG_TAG, "Init GUI called");

        gui = (Gui*)furi_record_open(RECORD_GUI);
        viewDispatcher = view_dispatcher_alloc();
        view_dispatcher_attach_to_gui(viewDispatcher, gui, ViewDispatcherTypeFullscreen);
    }

    void PushView(UiView* view) {
        if(!viewStack.empty()) {
            viewStack.top()->SetOnTop(false);
        }

        viewStack.push(view);
        view->SetOnTop(true);

        view_set_previous_callback(view->GetNativeView(), backCallback);
        view_dispatcher_add_view(viewDispatcher, currentViewId(), view->GetNativeView());
        view_dispatcher_switch_to_view(viewDispatcher, currentViewId());

        FURI_LOG_I(LOG_TAG, "ViewStack pushed, size: %d", viewStack.size());
    }

    void PopView(bool preserveView) {
        popView(preserveView);
        view_dispatcher_switch_to_view(viewDispatcher, currentViewId());
    }

    void RunEventLoop() {
        while(!viewStack.empty()) {
            FURI_LOG_I(LOG_TAG, "Running event loop");
            view_dispatcher_run(viewDispatcher);
        }
    }

    ~UiManager() {
        FURI_LOG_I(LOG_TAG, "Destructor called");

        while(!viewStack.empty()) {
            popView(false);
        }

        if(viewDispatcher != NULL) {
            view_dispatcher_free(viewDispatcher);
            viewDispatcher = NULL;
        }

        if(gui != NULL) {
            furi_record_close(RECORD_GUI);
            gui = NULL;
        }
    }
};
