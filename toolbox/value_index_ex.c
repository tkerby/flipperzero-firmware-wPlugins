#include "value_index_ex.h"

size_t value_index_display_mode(
    const TerminalDisplayMode value,
    const TerminalDisplayMode values[],
    size_t values_count) {
    size_t index = 0;

    for(size_t i = 0; i < values_count; i++) {
        if(value == values[i]) {
            index = i;
            break;
        }
    }

    return index;
}

size_t value_index_size_t(const size_t value, const size_t values[], size_t values_count) {
    return value_index_uint32(value, (uint32_t*)values, values_count);
}
