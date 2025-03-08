#ifndef _VARIABLE_ITEM_LIST_UI_VIEW_CLASS_
#define _VARIABLE_ITEM_LIST_UI_VIEW_CLASS_

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>

#include "UiView.cpp"
#include "item/UiVariableItem.cpp"

#undef LOG_TAG
#define LOG_TAG "UI_VARITEMLST"

using namespace std;

class VariableItemListUiView : public UiView {
private:
    VariableItemList* varItemList;

public:
    VariableItemListUiView() {
        varItemList = variable_item_list_alloc();
    }

    void AddItem(UiVariableItem* item) {
        item->AddTo(varItemList);
    }

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
