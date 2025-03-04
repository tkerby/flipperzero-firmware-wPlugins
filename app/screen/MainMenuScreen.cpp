#ifndef _MAIN_MENU_SCREEN_CLASS_
#define _MAIN_MENU_SCREEN_CLASS_

#include "../ui/view/UiView.cpp"
#include "../ui/view/SubMenuUiView.cpp"

class MainMenuScreen {
private:
    SubMenuUiView* menuView;

    void scanMenuPress(uint32_t index) {
        UNUSED(index);
        menuView->AddItem("You pressed scan!!", UI_HANDLER(&MainMenuScreen::otherMenuPress));
    }

    void otherMenuPress(uint32_t index) {
        UNUSED(index);
    }

    void handler(uint32_t index) {
        UNUSED(index);
        furi_delay_ms(1000);
    }

public:
    MainMenuScreen() {
        menuView = new SubMenuUiView();
        menuView->AddItem("Scan", UI_HANDLER(&MainMenuScreen::scanMenuPress));
        menuView->AddItem("Saved staion groups", UI_HANDLER(&MainMenuScreen::otherMenuPress));
        menuView->AddItem("Settings", UI_HANDLER(&MainMenuScreen::otherMenuPress));
        menuView->AddItem("About", UI_HANDLER(&MainMenuScreen::otherMenuPress));
        menuView->AddItem("TestHandler", UI_HANDLER(&MainMenuScreen::handler));
    }

    UiView* GetView() {
        return menuView;
    }

    ~MainMenuScreen() {
        delete menuView;
    }
};

#endif //_MAIN_MENU_SCREEN_CLASS_
