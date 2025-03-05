#ifndef _MAIN_MENU_SCREEN_CLASS_
#define _MAIN_MENU_SCREEN_CLASS_

#include "../ui/view/UiView.cpp"
#include "../ui/view/SubMenuUiView.cpp"
#include "../ui/UiManager.cpp"

#include "ScanStationsScreen.cpp"

class MainMenuScreen {
private:
    SubMenuUiView* menuView;

public:
    MainMenuScreen() {
        menuView = new SubMenuUiView("Chief Cooker");
        menuView->AddItem("Scan for station signals", UI_HANDLER_1ARG(&MainMenuScreen::scanStationsMenuPress));
        menuView->AddItem("Saved staions database", UI_HANDLER_1ARG(&MainMenuScreen::otherMenuPress));
        menuView->AddItem("About / Manual", UI_HANDLER_1ARG(&MainMenuScreen::otherMenuPress));
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void scanStationsMenuPress(uint32_t index) {
        UNUSED(index);
        UiManager::GetInstance()->PushView((new ScanStationsScreen())->GetView());
    }

    void otherMenuPress(uint32_t index) {
        menuView->SetItemLabel(index, "Your pushed me!");
    }
};

#endif //_MAIN_MENU_SCREEN_CLASS_
