#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "lib/ui/UiManager.hpp"
#include "lib/hardware/notification/Notification.hpp"
#include "AppFileSystem.hpp"

#include "AppConfig.hpp"
#include "app/screen/MainMenuScreen.hpp"

using namespace std;

class App {
public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        ui->InitGui();

        AppConfig* config = new AppConfig();

        MainMenuScreen* mainMenuScreen = new MainMenuScreen(config);
        ui->PushView(mainMenuScreen->GetView());
        ui->RunEventLoop();

        delete mainMenuScreen;
        delete config;
        delete ui;

        Notification::Dispose();
    }
};

#endif //_APP_CLASS_
