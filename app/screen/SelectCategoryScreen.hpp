#pragma once

#include "lib/file/FileManager.hpp"
#include "lib/ui/view/SubMenuUiView.hpp"

class SelectCategoryScreen {
private:
    SubMenuUiView* menu;
    function<void(const char*)> categorySelected = NULL;

public:
    SelectCategoryScreen(bool canCreateNew, function<void(const char*)> categorySelectedHandler) {
        this->categorySelected = categorySelectedHandler;

        menu = new SubMenuUiView("Select category");

        if(canCreateNew) {
            menu->AddItem("+ Create NEW", HANDLER_1ARG(&SelectCategoryScreen::createNew));
        }

        menu->AddItem("<Default/Uncategorized>", HANDLER_1ARG(&SelectCategoryScreen::selectDefault));

        FileManager fileManager = FileManager();
        
    }

private:
    void createNew(uint32_t) {
    }

    void selectDefault(uint32_t) {
        categorySelected(NULL);
    }
};
