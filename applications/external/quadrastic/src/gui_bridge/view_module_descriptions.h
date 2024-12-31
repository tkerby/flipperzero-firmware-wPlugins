#pragma once

typedef struct View View;

typedef void* (*ViewModuleAllocCallback)(void);
typedef void (*ViewModuleFreeCallback)(void* view_module);
typedef View* (*ViewModuleGetViewCallback)(void* view_module);

typedef struct
{
    ViewModuleAllocCallback alloc;
    ViewModuleFreeCallback free;
    ViewModuleGetViewCallback get_view;
} ViewModuleDescription;

extern const ViewModuleDescription variable_item_list_description;
