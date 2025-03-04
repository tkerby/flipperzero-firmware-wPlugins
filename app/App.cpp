#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "ui/UiManager.cpp"
#include "ui/view/SubMenuUiView.cpp"

class App {
private:
    UiManager* ui;

public:
    void Run() {
        ui = new UiManager();
        ui->InitGui();

        SubMenuUiView* mainMenu = new SubMenuUiView("Chief Cooker");
        mainMenu->AddItem("Scan", [](uint32_t index) { UNUSED(index); });
        mainMenu->AddItem("Saved staion groups", [](uint32_t index) { UNUSED(index); });
        mainMenu->AddItem("Settings", [](uint32_t index) { UNUSED(index); });
        mainMenu->AddItem("About", [](uint32_t index) { UNUSED(index); });

        ui->PushView(mainMenu);
        ui->RunEventLoop();
    }
};

#endif //_APP_CLASS_
