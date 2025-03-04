#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "ui/UiManager.cpp"
#include "ui/view/SubMenuUiView.cpp"

using namespace std;

class App {
private:
    SubMenuUiView* mainMenu;

public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        ui->InitGui();

        mainMenu = new SubMenuUiView("Chief Cooker");
        mainMenu->AddItem("Scan", UI_HANDLER(&App::scanMenuPress));
        mainMenu->AddItem("Saved staion groups", UI_HANDLER(&App::otherMenuPress));
        mainMenu->AddItem("Settings", UI_HANDLER(&App::otherMenuPress));
        mainMenu->AddItem("About", UI_HANDLER(&App::otherMenuPress));
        mainMenu->AddItem("TestHandler", UI_HANDLER(&App::handler));

        ui->PushView(mainMenu);
        ui->RunEventLoop();

        delete ui;
    }

    void scanMenuPress(uint32_t index) {
        UNUSED(index);
        mainMenu->AddItem("You pressed scan!!", UI_HANDLER(&App::otherMenuPress));
    }

    void otherMenuPress(uint32_t index) {
        UNUSED(index);
    }

    void handler(uint32_t index) {
        UNUSED(index);
        furi_delay_ms(1000);
    }
};

#endif //_APP_CLASS_
