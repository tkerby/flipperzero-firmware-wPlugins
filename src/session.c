#include <furi.h>
#include "session.h"

uint8_t EnsureLoadedSession(const PSESSION session) {
	if(!session) {
		return 0;
	}

	return 1;
}
