#include "wav_reader.h"

#include <string.h>

static uint16_t le16(const uint8_t* p) {
    return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool wav_read_header(WavReadAt read, void* context, uint32_t size, WavInfo* info) {
    uint8_t header[16];
    memset(info, 0, sizeof(*info));
    if(size < 12 || !read(context, 0, header, 12) || memcmp(header, "RIFF", 4) ||
       memcmp(header + 8, "WAVE", 4))
        return false;
    uint32_t riff_size = le32(header + 4);
    if(riff_size < 4 || riff_size > size - 8) return false;
    uint32_t end = riff_size + 8;
    bool format_found = false, data_found = false;
    for(uint32_t offset = 12; offset <= end && end - offset >= 8;) {
        if(!read(context, offset, header, 8)) return false;
        uint32_t length = le32(header + 4);
        offset += 8;
        if(length > end - offset) return false;
        if(memcmp(header, "fmt ", 4) == 0) {
            if(format_found || length < 16 || !read(context, offset, header, 16)) return false;
            uint32_t rate = le32(header + 4);
            if(le16(header) != 1 || le16(header + 2) != 1 || le16(header + 14) != 8 ||
               le16(header + 12) != 1 || le32(header + 8) != rate || rate < 8000 || rate > 48000)
                return false;
            info->sample_rate = rate;
            format_found = true;
        } else if(memcmp(header, "data", 4) == 0) {
            if(data_found || !length) return false;
            info->data_offset = offset;
            info->sample_count = length;
            data_found = true;
        }
        if(format_found && data_found) return true;
        if((length & 1) && length == end - offset) return false;
        offset += length + (length & 1);
    }
    return false;
}
