#ifndef _PAGER_ACTION_ENUM_
#define _PAGER_ACTION_ENUM_

enum PagerAction {
    UNKNOWN,
    RING,
    MUTE,
    DESYNC,
    TURN_OFF_ALL,
};

// static bool IsPagerActionSpecial(PagerAction action) {
//     switch(action) {
//     case DESYNC:
//     case TURN_OFF_ALL:
//         return true;

//     default:
//         return false;
//     }
// }

#endif //_PAGER_ACTION_ENUM_
