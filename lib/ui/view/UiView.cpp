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

    static bool onInput(InputEvent* input, void* context) {
        auto handlerContext = (HandlerContext<function<bool(InputEvent*)>>*)context;
        return handlerContext->GetHandler()(input);
    }

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

    void SetInputHandler(function<bool(InputEvent*)> handler) {
        view_set_context(GetNativeView(), new HandlerContext(handler));
        view_set_input_callback(GetNativeView(), onInput);
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
