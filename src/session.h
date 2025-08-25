#include <stdint.h>

typedef struct {
} SESSION, *PSESSION;

#ifdef __cplusplus
extern "C" {
#endif
void SessionAllocate(const PSESSION session);
void SessionFree(const PSESSION session);
//uint8_t SessionOpen(const PSESSION session, const char* const path);
//void SessionCleanup(const PSESSION session);
#ifdef __cplusplus
}
#endif
