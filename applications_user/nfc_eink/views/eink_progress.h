#pragma once

#include <stdint.h>
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EinkProgress EinkProgress;

EinkProgress* eink_progress_alloc(void);
void eink_progress_free(EinkProgress* instance);
View* eink_progress_get_view(EinkProgress* instance);

#ifdef __cplusplus
}
#endif
