#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>

#include "UiView.hpp"
#include "lib/HandlerContext.hpp"
#include <list>

#undef LOG_TAG
#define LOG_TAG "UI_SUBMENU"

using namespace std;

class SubMenuUiView : public UiView {
private:
    Submenu* menu;
    uint32_t elementCount = 0;
    list<IDestructable*> handlers;

    static void executeCallback(void* context, uint32_t index) {
        if(context == NULL) {
            return;
        }

        HandlerContext<function<void(uint32_t)>>* handlerContext = (HandlerContext<function<void(uint32_t)>>*)context;
        handlerContext->GetHandler()(index);
    }

public:
    SubMenuUiView() {
        menu = submenu_alloc();
    }

    SubMenuUiView(const char* header) : SubMenuUiView() {
        SetHeader(header);
    }

    void SetHeader(const char* header) {
        submenu_set_header(menu, header);
    }

    void AddItem(const char* label, function<void(uint32_t)> handler) {
        AddItemAt(elementCount, label, handler);
    }

    void AddItemAt(uint32_t index, const char* label, function<void(uint32_t)> handler) {
        auto handlerContext = new HandlerContext(handler);
        handlers.push_back(handlerContext);
        submenu_add_item(menu, label, index, executeCallback, handlerContext);
        elementCount++;
    }

    void SetItemLabel(uint32_t index, const char* label) {
        if(index >= elementCount) {
        }
        submenu_change_item_label(menu, index, label);
    }

    View* GetNativeView() {
        return submenu_get_view(menu);
    }

    uint32_t GetCurrentIndex() {
        return submenu_get_selected_item(menu);
    }

    ~SubMenuUiView() {
        if(menu != NULL) {
            OnDestory();

            for(IDestructable* handlerContext : handlers) {
                delete handlerContext;
            }

            submenu_free(menu);
            menu = NULL;
        }
    }
};
