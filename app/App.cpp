#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "lib/ui/UiManager.cpp"
#include "lib/hardware/notification/Notification.cpp"

#include "app/screen/MainMenuScreen.cpp"

using namespace std;

class App {
public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        ui->InitGui();

        MainMenuScreen mainMenuScreen;
        ui->PushView(mainMenuScreen.GetView());
        ui->RunEventLoop();

        ui->Destroy();

        Notification::Dispose();
    }
};

#endif //_APP_CLASS_
