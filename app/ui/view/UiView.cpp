#ifndef _UI_VIEW_CLASS_
#define _UI_VIEW_CLASS_

#include <gui/gui.h>
#include <gui/view.h>

class UiView {
public:
    virtual View* GetNativeView() = 0;
    virtual ~UiView() {
    }
};

#endif //_UI_VIEW_CLASS_
