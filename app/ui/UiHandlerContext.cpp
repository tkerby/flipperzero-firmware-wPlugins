#ifndef _UI_HANDLER_CONTEXT_CLASS_
#define _UI_HANDLER_CONTEXT_CLASS_

#define UI_HANDLER(handlerMethod) bind(handlerMethod, this, placeholders::_1)

template <class T>
class UiHandlerContext {
private:
    T handler;

public:
    UiHandlerContext(T handler) {
        this->handler = handler;
    }

    T GetHandler() {
        return handler;
    }
};

#endif //_UI_HANDLER_CONTEXT_CLASS_
