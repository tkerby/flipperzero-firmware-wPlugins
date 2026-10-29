#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Зарегистрировать CLI-команду `tea_timer_test`. */
void tea_timer_tests_register(void);

/** Снять CLI-команду `tea_timer_test`. */
void tea_timer_tests_unregister(void);

#ifdef __cplusplus
}
#endif
