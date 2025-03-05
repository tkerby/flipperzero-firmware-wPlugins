#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"

class ScanStationsScreen {
private:
    SubMenuUiView* menuView;
    SubGhzModule* subghz;

public:
    ScanStationsScreen() {
        subghz = new SubGhzModule();

        menuView = new SubMenuUiView("Scanning for signals...");
        menuView->SetOnDestroyHandler(UI_HANDLER(&ScanStationsScreen::destroy));
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void destroy() {
        delete subghz;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
