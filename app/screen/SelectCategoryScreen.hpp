#pragma once

#include "app/AppFileSystem.hpp"
#include "lib/ui/view/SubMenuUiView.hpp"

class SelectCategoryScreen {
private:
    SubMenuUiView* menu;
    CategoryType categoryType;
    forward_list<char*> categories;
    function<void(CategoryType, const char*)> categorySelectedHandler;

public:
    SelectCategoryScreen(
        bool canCreateNew,
        CategoryType categoryType,
        function<void(CategoryType, const char*)> categorySelectedHandler
    ) {
        this->categoryType = categoryType;
        this->categorySelectedHandler = categorySelectedHandler;

        menu = new SubMenuUiView("Select category");
        menu->SetOnDestroyHandler(HANDLER(&SelectCategoryScreen::destory));

        if(canCreateNew) {
            menu->AddItem("+ Create NEW", HANDLER_1ARG(&SelectCategoryScreen::createNew));
        }

        if(categoryType == User) {
            menu->AddItem("<Default/Uncategorized>", [categoryType, categorySelectedHandler](uint32_t) {
                return categorySelectedHandler(categoryType, NULL);
            });
        }

        AppFileSysytem().GetCategories(&categories, categoryType);
        for(char* category : categories) {
            menu->AddItem(category, [categoryType, category, categorySelectedHandler](uint32_t) {
                return categorySelectedHandler(categoryType, category);
            });
        }
    }

    UiView* GetView() {
        return menu;
    }

private:
    void createNew(uint32_t) {
        //TODO
    }

    void destory() {
        while(!categories.empty()) {
            delete[] categories.front();
            categories.pop_front();
        }
    }
};
