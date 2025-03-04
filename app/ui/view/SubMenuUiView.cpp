#ifndef _SUBMENU_UI_VIEW_CLASS_
#define _SUBMENU_UI_VIEW_CLASS_

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include "UiView.cpp"

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
    }

public:
    SubMenuUiView(const char* header) {
        menu = submenu_alloc();
        submenu_set_header(menu, header);
    }

    void AddItem(const char* label, void callback(void*, uint32_t), void* context) {
        AddItemAt(elementCount, label, callback, context);
    }

    void AddItemAt(int index, const char* label, void callback(void*, uint32_t), void* context) {
        submenu_add_item(menu, label, index, callback, context);
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
