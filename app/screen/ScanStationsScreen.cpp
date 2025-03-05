#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"

class ScanStationsScreen {
private:
    SubMenuUiView* menuView;

public:
    ScanStationsScreen() {
        menuView = new SubMenuUiView("Scanning for signals...");
        menuView->SetOnDestroyHandler(UI_HANDLER(&ScanStationsScreen::destroy));
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void destroy() {
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
