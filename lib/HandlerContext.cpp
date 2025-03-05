#ifndef _HANDLER_CONTEXT_CLASS_
#define _HANDLER_CONTEXT_CLASS_

#include <functional>

using namespace std;

#define HANDLER(handlerMethod)      bind(handlerMethod, this)
#define HANDLER_1ARG(handlerMethod) bind(handlerMethod, this, placeholders::_1)

template <class T>
class HandlerContext {
private:
    T handler;

public:
    HandlerContext(T handler) {
        this->handler = handler;
    }

    T GetHandler() {
        return handler;
    }
};

#endif //_HANDLER_CONTEXT_CLASS_
