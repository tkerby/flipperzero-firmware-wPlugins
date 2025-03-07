#ifndef _HELPERS_CLASS_
#define _HELPERS_CLASS_

#include <core/string.h>

class Helpers {
public:
    static const char* intToString(int value) {
        return intToString(value, "%d");
    }

    static const char* intToString(int value, const char* format) {
        FuriString* str = furi_string_alloc_printf(format, value);
        return furi_string_get_cstr(str);
    }
};

#endif //_HELPERS_CLASS_
