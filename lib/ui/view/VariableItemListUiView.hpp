#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>

#include "UiView.hpp"
#include "item/UiVariableItem.hpp"

#undef LOG_TAG
#define LOG_TAG "UI_VARITEMLST"

using namespace std;

class VariableItemListUiView : public UiView {
private:
    uint32_t itemCounter = 0;
    VariableItemList* varItemList;
    IDestructable* enterPressHandler;

    static void onEnterCallback(void* context, uint32_t index) {
        auto handlerContext = (HandlerContext<function<void(uint32_t)>>*)context;
        handlerContext->GetHandler()(index);
    }

public:
    VariableItemListUiView() {
        varItemList = variable_item_list_alloc();
    }

    uint32_t AddItem(UiVariableItem* item) {
        item->AddTo(varItemList);
        return itemCounter++;
    }

    void SetEnterPressHandler(function<void(uint32_t)> handler) {
        if(enterPressHandler != NULL) {
            delete enterPressHandler;
        }
        variable_item_list_set_enter_callback(varItemList, onEnterCallback, enterPressHandler = new HandlerContext(handler));
    }

    View* GetNativeView() {
        return variable_item_list_get_view(varItemList);
    }

    ~VariableItemListUiView() {
        if(varItemList != NULL) {
            OnDestory();

            if(enterPressHandler != NULL) {
                delete enterPressHandler;
            }

            variable_item_list_free(varItemList);
            varItemList = NULL;
        }
    }
};
