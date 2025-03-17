#pragma once

#include "app/AppConfig.hpp"

#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/SubMenuUiView.hpp"
#include "lib/ui/UiManager.hpp"

#include "app/AppNotifications.hpp"

#include "ScanStationsScreen.hpp"

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
    void scanStationsMenuPressed(uint32_t) {
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config, true, NULL, true))->GetView());
    }

    void stationDatabasePressed(uint32_t) {
        SubMenuUiView* savedMenuView = new SubMenuUiView();
        savedMenuView->AddItem("Manually saved stations", HANDLER_1ARG(&MainMenuScreen::savedStationsPressed));

        FileManager fileManager = FileManager();
        Directory* autosaved = fileManager.OpenDirectory(AUTOSAVED_STATIONS_PATH);
        if(autosaved != NULL) {
            char dirName[AUTOSAVED_DIR_NAME_LENGTH];
            while(autosaved->GetNextFile(dirName, AUTOSAVED_DIR_NAME_LENGTH)) {
                String itemName = String("Autosaved %s", dirName);
                savedMenuView->AddItem(itemName.cstr(), [this, dirName](uint32_t) { autosavedStationsPressed(dirName); });
            }
        }
        delete autosaved;

        UiManager::GetInstance()->PushView(savedMenuView);
    }

    void savedStationsPressed(uint32_t) {
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config, false, SAVED_STATIONS_PATH, false))->GetView());
    }

    void autosavedStationsPressed(const char* dirName) {
        String path = String("%s/%s", AUTOSAVED_STATIONS_PATH, dirName);
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config, false, path.cstr(), true))->GetView());
    }

    void aboutPressed(uint32_t index) {
        UNUSED(index);

        Notification::Play(&NOTIFICATION_PAGER_RECEIVE);
        menuView->SetItemLabel(index, "Developed by Denr01!");
    }

    void destroy() {
        delete this;
    }
};
