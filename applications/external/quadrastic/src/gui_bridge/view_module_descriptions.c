#include "view_module_descriptions.h"

#include "gui/modules/variable_item_list.h"

const ViewModuleDescription variable_item_list_description = {
    .alloc = (ViewModuleAllocCallback)variable_item_list_alloc,
    .free = (ViewModuleFreeCallback)variable_item_list_free,
    .get_view = (ViewModuleGetViewCallback)variable_item_list_get_view,
};
