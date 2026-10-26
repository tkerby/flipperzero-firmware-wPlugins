#include "nb_backend.h"

NbTagType nb_tag_type(const NbTag* t) {
    if(t->ats_len > 0 || (t->sak & 0x20)) return NbTagIso4a;
    switch(t->sak & 0x3F) {
    case 0x09:
        return NbTagMfcMini;
    case 0x08:
    case 0x88: /* Infineon 1K */
        return NbTagMfc1k;
    case 0x18:
        return NbTagMfc4k;
    case 0x00:
        return NbTagUltralight;
    default:
        return NbTagUnknown;
    }
}
