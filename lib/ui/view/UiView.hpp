#pragma once

#include <functional>

#include <gui/gui.h>
#include <gui/view.h>

#include "lib/HandlerContext.hpp"

using namespace std;

class UiView {
private:
    function<void()> onDestroyHandler = HANDLER(&UiView::doNothing);
    function<void()> onReturnToView = HANDLER(&UiView::doNothing);
    function<bool()> goBackHandler = []() { return true; };
    IDestructable* inputHandler = NULL;
    bool isOnTop = false;

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
        view_set_context(GetNativeView(), inputHandler = new HandlerContext(handler));
        view_set_input_callback(GetNativeView(), onInput);
    }

    void SetGoBackHandler(function<bool()> handler) {
        goBackHandler = handler;
    }

    void OnReturn() {
        onReturnToView();
    }

    bool IsOnTop() {
        return isOnTop;
    }

    void SetOnTop(bool value) {
        this->isOnTop = value;
    }

    bool GoBack() {
        return goBackHandler();
    }

protected:
    // Must be called from parent class destructor's!
    void OnDestory() {
        if(inputHandler != NULL) {
            delete inputHandler;
            inputHandler = NULL;
        }
        onDestroyHandler();
    }
};
