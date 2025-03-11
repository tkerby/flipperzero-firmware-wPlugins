#ifndef _BATCH_TRANSMISSION_SCREEN_CLASS_
#define _BATCH_TRANSMISSION_SCREEN_CLASS_

#include "app/pager/PagerDataStored.cpp"
#include "app/pager/decoder/PagerDecoder.cpp"
#include "lib/hardware/subghz/SubGhzModule.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/ProgressbarPopupUiView.cpp"

class BatchTransmissionScreen {
private:
    ProgressbarPopupUiView* popup;

    PagerDataStored* pager;
    PagerDecoder* decoder;
    SubGhzModule* subghz;

public:
    BatchTransmissionScreen(PagerDataStored* pager, PagerDecoder* decoder, SubGhzModule* subghz) {
        this->pager = pager;
        this->decoder = decoder;
        this->subghz = subghz;

        popup = new ProgressbarPopupUiView("Transmitting...");
        popup->SetProgress("Pager 0 / 50", 0);
        popup->SetOnDestroyHandler(HANDLER(&BatchTransmissionScreen::destroy));
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
