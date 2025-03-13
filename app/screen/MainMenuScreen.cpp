#ifndef _MAIN_MENU_SCREEN_CLASS_
#define _MAIN_MENU_SCREEN_CLASS_

#include "app/AppConfig.cpp"

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"
#include "lib/ui/UiManager.cpp"

#include "app/AppNotifications.cpp"

#include "ScanStationsScreen.cpp"

class MainMenuScreen {
private:
    AppConfig* config;
    SubMenuUiView* menuView;

public:
    MainMenuScreen(AppConfig* config) {
        this->config = config;

        menuView = new SubMenuUiView("Chief Cooker");
        menuView->AddItem("Scan for station signals", HANDLER_1ARG(&MainMenuScreen::scanStationsMenuPressed));
        menuView->AddItem("Saved staions database", HANDLER_1ARG(&MainMenuScreen::stationDatabasePressed));
        menuView->AddItem("About / Manual", HANDLER_1ARG(&MainMenuScreen::aboutPressed));
        menuView->SetOnDestroyHandler(HANDLER(&MainMenuScreen::destroy));
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void scanStationsMenuPressed(uint32_t index) {
        UNUSED(index);
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config))->GetView());
    }

    void stationDatabasePressed(uint32_t index) {
        UNUSED(index);
    }

    void aboutPressed(uint32_t index) {
        UNUSED(index);

        Notification::Play(&NOTIFICATION_PAGER_RECEIVE);
        menuView->SetItemLabel(index, "Your pushed me!");
    }

    void destroy() {
        delete this;
    }
};

#endif //_MAIN_MENU_SCREEN_CLASS_
