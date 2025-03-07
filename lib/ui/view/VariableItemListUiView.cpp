#ifndef _VARIABLE_ITEM_LIST_UI_VIEW_CLASS_
#define _VARIABLE_ITEM_LIST_UI_VIEW_CLASS_

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>

#include "UiView.cpp"
#include "lib/HandlerContext.cpp"
#include "item/UiVariableItem.cpp"

#undef LOG_TAG
#define LOG_TAG "UI_VARITEMLST"

using namespace std;

class VariableItemListUiView : public UiView {
private:
    VariableItemList* varItemList;

    static void executeCallback(void* context, uint32_t index) {
        if(context == NULL) {
            FURI_LOG_W(LOG_TAG, "SubMenuUiView element %d has NULL handler!", (int)index);
            return;
        }

        HandlerContext<function<void(uint32_t)>>* handlerContext = (HandlerContext<function<void(uint32_t)>>*)context;
        handlerContext->GetHandler()(index);
    }

public:
    VariableItemListUiView() {
        varItemList = variable_item_list_alloc();
    }

    void AddItem(UiVariableItem* item) {
        item->AddTo(varItemList);
    }

    // void AddItemAt(uint32_t index, const char* label, function<void(uint32_t)> handler) {
    //     submenu_add_item(menu, label, index, executeCallback, new HandlerContext(handler));
    //     elementCount++;
    // }

    // void SetItemLabel(uint32_t index, const char* label) {
    //     if(index >= elementCount) {
    //         FURI_LOG_W(LOG_TAG, "Cannot modify name of non-existing item %d to %s", (int)index, label);
    //     }
    //     submenu_change_item_label(menu, index, label);
    // }

    View* GetNativeView() {
        return variable_item_list_get_view(varItemList);
    }

    ~VariableItemListUiView() {
        if(varItemList != NULL) {
            OnDestory();
            variable_item_list_free(varItemList);
            varItemList = NULL;
        }
    }
};

#endif //_VARIABLE_ITEM_LIST_UI_VIEW_CLASS_
