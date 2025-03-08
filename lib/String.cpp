#ifndef _STRING_CLASS_
#define _STRING_CLASS_

#include <core/string.h>

class String {
private:
    FuriString* string;

public:
    String() {
        string = furi_string_alloc();
    }

    String(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string = furi_string_alloc_vprintf(format, args);
        va_end(args);
    }

    const char* cstr() {
        return furi_string_get_cstr(string);
    }

    const char* fromInt(int value) {
        return format("%d", value);
    }

    const char* format(const char* format, ...) {
        va_list args;
        va_start(args, format);
        furi_string_vprintf(string, format, args);
        va_end(args);

        return cstr();
    }

    ~String() {
        furi_string_free(string);
    }
};

#endif //_STRING_CLASS_
