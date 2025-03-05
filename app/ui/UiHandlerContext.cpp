#ifndef _UI_HANDLER_CONTEXT_CLASS_
#define _UI_HANDLER_CONTEXT_CLASS_

#include <functional>

using namespace std;

#define UI_HANDLER(handlerMethod)      bind(handlerMethod, this)
#define UI_HANDLER_1ARG(handlerMethod) bind(handlerMethod, this, placeholders::_1)

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
