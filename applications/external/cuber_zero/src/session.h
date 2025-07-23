#ifndef __SESSION_H__
#define __SESSION_H__

typedef struct {
} SESSION, *PSESSION;

#ifdef __cplusplus
extern "C" {
#endif
void EnsureLoadedSession(const PSESSION session);
#ifdef __cplusplus
}
#endif
#endif
