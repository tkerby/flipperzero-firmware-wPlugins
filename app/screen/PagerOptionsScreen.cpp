#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/VariableItemListUiView.cpp"

#include "lib/ui/UiManager.cpp"

class PagerOptionsScreen {
private:
    VariableItemListUiView* varItemList;

public:
    PagerOptionsScreen() {
        varItemList = new VariableItemListUiView();

        UiVariableItem* encodingItem = new UiVariableItem("Encoding", decoder.id, decoders.count(), encodingValueChanged);

        varItemList->AddItem(encodingItem);

        // varItemList->AddItem(UiVariableItem *item, function<void (uint32_t)> changeHandler)
        menuView->AddItem("Scan for station signals", HANDLER_1ARG(&MainMenuScreen::scanStationsMenuPressed));
        menuView->AddItem("Saved staions database", HANDLER_1ARG(&MainMenuScreen::stationDatabasePressed));
        menuView->AddItem("About / Manual", HANDLER_1ARG(&MainMenuScreen::aboutPressed));
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    const char* encodingValueChanged(uint32_t index) {
        UNUSED(index);
        return decoders[index].GetShortName();
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
