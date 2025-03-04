#ifndef _APP_CLASS_
#define _APP_CLASS_

#include "ui/UiManager.cpp"
#include "ui/view/SubMenuUiView.cpp"

class App {
public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        ui->InitGui();

        SubMenuUiView* mainMenu = new SubMenuUiView("Chief Cooker");
        mainMenu->AddItem("Scan", scanMenuPress, mainMenu);
        mainMenu->AddItem("Saved staion groups", otherMenuPress, this);
        mainMenu->AddItem("Settings", otherMenuPress, this);
        mainMenu->AddItem("About", otherMenuPress, this);

        ui->PushView(mainMenu);
        ui->RunEventLoop();

        delete ui;
    }

    static void scanMenuPress(void* subMenuUiView, uint32_t index) {
        SubMenuUiView* mainMenu = (SubMenuUiView*)subMenuUiView;
        mainMenu->AddItem("You pressed scan!!", otherMenuPress, mainMenu);
        UNUSED(index);
    }

    static void otherMenuPress(void* app, uint32_t index) {
        UNUSED(app);
        UNUSED(index);
    }
};

#endif //_APP_CLASS_
