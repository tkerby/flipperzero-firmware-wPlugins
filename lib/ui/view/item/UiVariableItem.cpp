#ifndef _UI_VARIABLE_ITEM_CLASS_
#define _UI_VARIABLE_ITEM_CLASS_

#include <functional>

#include "gui/modules/variable_item_list.h"

using namespace std;

class UiVariableItem {
private:
    const char* label;

public:
    UiVariableItem(const char* label, int selectedItem, int itemCount, function<const char*(int)> changeHandler) {
        this->label = label;
    }

    void Bind() {
    }

    void SetValueChangeHandler(function<const char*(int)> s) {
        variable_ite
    }

    void SetSelectedItem(int selectedItem, int itemCount) {
        // variable_item_set_current_value_text(VariableItem *item, const char *current_value_text)
    }

    const char* GetLabel() {
        return label;
    }

    int GetValuesCount() {
    }

    static void itemChangeCallback(VariableItem* item) {
        variable_item_get_context(VariableItem * item)
    }
};

#endif //_UI_VARIABLE_ITEM_CLASS_
