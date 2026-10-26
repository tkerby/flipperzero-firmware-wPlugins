#pragma once

#include "include/domain/wc_compare.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Load two capture files (names including .wcen) and compare them. The two censuses are
// heap-allocated (each is large) so this never blows the small FAP stack. Returns false if
// either file fails to load.
bool wc_compare_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                            WcCompareResult *out);
