#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>

typedef struct {
} SESSION, *PSESSION;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t EnsureLoadedSession(const PSESSION session);
#ifdef __cplusplus
}
#endif
#endif
