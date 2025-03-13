#ifndef _BATCH_TRANSMISSION_SCREEN_CLASS_
#define _BATCH_TRANSMISSION_SCREEN_CLASS_

#include "lib/String.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/ProgressbarPopupUiView.cpp"

class BatchTransmissionScreen {
private:
    ProgressbarPopupUiView* popup;
    String statusStr;

public:
    BatchTransmissionScreen() {
        popup = new ProgressbarPopupUiView("Transmitting...");
        popup->SetProgress("Pager 0 / 50", 0);
        popup->SetOnDestroyHandler(HANDLER(&BatchTransmissionScreen::destroy));
    }

    void SetProgress(int pagerNum, int pagersTotal) {
        float progressValue = (float)pagerNum / pagersTotal;
        popup->SetProgress(statusStr.format("Pager %d / %d", pagerNum, pagersTotal), progressValue);
    }

private:
    void destroy() {
        delete this;
    }

public:
    UiView* GetView() {
        return popup;
    }
};

#endif //_BATCH_TRANSMISSION_SCREEN_CLASS_
