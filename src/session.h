#include <storage/storage.h>

typedef struct {
	FuriString* path;
	File* file;
} SESSION, *PSESSION;

#ifdef __cplusplus
extern "C" {
#endif
void SessionFree(const PSESSION session);
void SessionInitialize(const PSESSION session);
//uint8_t SessionOpen(const PSESSION session, const char* const path);
//void SessionCleanup(const PSESSION session);
#ifdef __cplusplus
}
#endif
