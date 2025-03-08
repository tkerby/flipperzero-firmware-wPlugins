#ifndef _UI_VIEW_CLASS_
#define _UI_VIEW_CLASS_

#include <functional>

#include <gui/gui.h>
#include <gui/view.h>

#include "lib/HandlerContext.cpp"

using namespace std;

class UiView {
private:
    function<void()> onDestroyHandler = HANDLER(&UiView::doNothing);
    function<void()> onReturnToView = HANDLER(&UiView::doNothing);

    void doNothing() {
    }

public:
    virtual View* GetNativeView() = 0;
    virtual ~UiView() {
    }

    void SetOnDestroyHandler(function<void()> handler) {
        onDestroyHandler = handler;
    }

    void SetOnReturnToViewHandler(function<void()> handler) {
        onReturnToView = handler;
    }

    void OnReturn() {
        onReturnToView();
    }

protected:
    // Must be called from parent class destructor's!
    void OnDestory() {
        onDestroyHandler();
    }
};

#endif //_UI_VIEW_CLASS_
