#ifndef _SUBMENU_UI_VIEW_CLASS_
#define _SUBMENU_UI_VIEW_CLASS_

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include "UiView.cpp"

#include <functional>
#include <vector>
#include "../UiHandlerContext.cpp"

using namespace std;

class SubMenuUiView : public UiView {
private:
    Submenu* menu;
    int elementCount = 0;

    static void executeCallback(void* context, uint32_t index) {
        if(context == NULL) {
            FURI_LOG_W("CHCKR", "SubMenuUiView element %d has NULL handler!", (int)index);
            return;
        }

        UiHandlerContext<function<void(uint32_t)>>* handlerContext = (UiHandlerContext<function<void(uint32_t)>>*)context;
        handlerContext->GetHandler()(index);
    }

public:
    SubMenuUiView() {
        menu = submenu_alloc();
    }

    SubMenuUiView(const char* header) : SubMenuUiView() {
        submenu_set_header(menu, header);
    }

    void AddItem(const char* label, function<void(uint32_t)> handler) {
        AddItemAt(elementCount, label, handler);
    }

    void AddItemAt(int index, const char* label, function<void(uint32_t)> handler) {
        submenu_add_item(menu, label, index, executeCallback, new UiHandlerContext(handler));
        elementCount++;
    }

    View* GetNativeView() {
        return submenu_get_view(menu);
    }

    ~SubMenuUiView() {
        submenu_free(menu);
    }
};

#endif //_SUBMENU_UI_VIEW_CLASS_
