#ifndef _SUBMENU_UI_VIEW_CLASS_
#define _SUBMENU_UI_VIEW_CLASS_

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <functional>
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

        function<void(int)>* callback = (function<void(int)>*)context;
        (*callback)(index);
    }

public:
    SubMenuUiView(const char* header) {
        menu = submenu_alloc();
        submenu_set_header(menu, header);
    }

    void AddItem(const char* label, function<void(int)> callback) {
        AddItemAt(elementCount, label, callback);
    }

    void AddItemAt(int index, const char* label, function<void(int)> callback) {
        submenu_add_item(menu, label, index, executeCallback, &callback);
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
