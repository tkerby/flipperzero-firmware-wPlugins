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

class AdvancedSubMenuUiView : public UiView {
private:
    Submenu* menu;
    uint32_t elementCount = 0;

    static void executeCallback(void* context, uint32_t index) {
        if(context == NULL) {
            FURI_LOG_W(LOG_TAG, "SubMenuUiView element %d has NULL handler!", (int)index);
            return;
        }

        HandlerContext<function<void(uint32_t)>>* handlerContext = (HandlerContext<function<void(uint32_t)>>*)context;
        handlerContext->GetHandler()(index);
    }

public:
    AdvancedSubMenuUiView() {
        menu = submenu_alloc();
    }

    AdvancedSubMenuUiView(const char* header) : SubMenuUiView() {
        SetHeader(header);
    }

    void SetHeader(const char* header) {
        submenu_set_header(menu, header);
    }

    void AddItem(const char* label, function<void(uint32_t)> handler) {
        AddItemAt(elementCount, label, handler);
    }

    void AddItemAt(uint32_t index, const char* label, function<void(uint32_t)> handler) {
        submenu_add_item(menu, label, index, executeCallback, new HandlerContext(handler));
        elementCount++;
    }

    void SetItemLabel(uint32_t index, const char* label) {
        if(index >= elementCount) {
            FURI_LOG_W(LOG_TAG, "Cannot modify name of non-existing item %d to %s", (int)index, label);
        }
        submenu_change_item_label(menu, index, label);
    }

    View* GetNativeView() {
        return submenu_get_view(menu);
    }

    uint32_t GetCurrentIndex() {
        return submenu_get_selected_item(menu);
    }

    ~AdvancedSubMenuUiView() {
        if(menu != NULL) {
            OnDestory();
            submenu_free(menu);
            menu = NULL;
        }
    }
};

#endif //_ADVANCED_SUBMENU_UI_VIEW_CLASS_
