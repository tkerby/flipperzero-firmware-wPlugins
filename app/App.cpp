#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "ui/UiManager.cpp"
#include "screen/MainMenuScreen.cpp"

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
    }
};

#endif //_APP_CLASS_
