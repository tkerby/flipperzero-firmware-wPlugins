#include "../../minunit.h"

#include "../../../tea_timer.h"
#include "../../../lib/tea_logic/brew_math.h"

/** Вспомогательное: сессия по пресету с заданными параметрами. */
static BrewSession brew_make_session(uint16_t first, uint16_t increment, uint8_t max_pours) {
    TeaPreset preset;
    memset(&preset, 0, sizeof(preset));
    snprintf(preset.name, sizeof(preset.name), "Test Tea");
    preset.leaf_tenths_g = 50;
    preset.first_pour_sec = first;
    preset.increment_sec = increment;
    preset.max_pours = max_pours;

    BrewSession session;
    brew_session_reset_from_preset(&preset, &session);
    return session;
}

MU_TEST(test_pour_sec_adds_increment) {
    mu_assert_int_eq(25, brew_math_pour_sec(20, 5));
    mu_assert_int_eq(20, brew_math_pour_sec(20, 0));
    mu_assert_int_eq(5, brew_math_pour_sec(0, 5));
}

MU_TEST(test_pour_sec_clamps_to_max) {
    mu_assert_int_eq(BREW_NEXT_POUR_MAX, brew_math_pour_sec(BREW_NEXT_POUR_MAX, 5));
    mu_assert_int_eq(63000, BREW_NEXT_POUR_MAX);
    mu_assert_int_eq(63000, brew_math_pour_sec(62999, 600));
}

MU_TEST(test_format_time) {
    char buffer[16];

    brew_math_format_time(20, buffer, sizeof(buffer));
    mu_assert_string_eq("00:20", buffer);

    brew_math_format_time(65, buffer, sizeof(buffer));
    mu_assert_string_eq("01:05", buffer);

    brew_math_format_time(3599, buffer, sizeof(buffer));
    mu_assert_string_eq("59:59", buffer);

    brew_math_format_time(3600, buffer, sizeof(buffer));
    mu_assert_string_eq("01:00:00", buffer);

    brew_math_format_time(63000, buffer, sizeof(buffer));
    mu_assert_string_eq("17:30:00", buffer);
}

MU_TEST(test_reset_from_preset) {
    BrewSession session = brew_make_session(20, 5, 6);

    mu_assert("has_preset must be true", session.has_preset);
    mu_assert_int_eq(0, session.completed_pours);
    mu_assert_int_eq(20, session.next_pour_sec);
    mu_assert_int_eq(BrewPhaseIDLE, session.phase);
    mu_assert_int_eq(6, session.preset.max_pours);
    mu_assert_int_eq(50, session.preset.leaf_tenths_g);
}

MU_TEST(test_reset_keeps_water_temp) {
    // FR-026: диапазон температуры сохраняется в сессии после запуска.
    // FR-012/FR-027: на экран заваривания значение не выводится, но не теряется.
    TeaPreset preset;
    memset(&preset, 0, sizeof(preset));
    snprintf(preset.name, sizeof(preset.name), "Temp Tea");
    preset.leaf_tenths_g = 50;
    preset.water_temp_min_c = 85;
    preset.water_temp_max_c = 87;
    preset.first_pour_sec = 20;
    preset.increment_sec = 5;
    preset.max_pours = 6;

    BrewSession session;
    brew_session_reset_from_preset(&preset, &session);

    mu_assert("has_preset must be true", session.has_preset);
    mu_assert_int_eq(85, session.preset.water_temp_min_c);
    mu_assert_int_eq(87, session.preset.water_temp_max_c);
}

MU_TEST(test_complete_increments_only_on_running) {
    BrewSession session = brew_make_session(20, 5, 6);

    // В IDLE завершение невозможно.
    mu_assert("complete in IDLE must return false", !brew_session_complete(&session));
    mu_assert_int_eq(0, session.completed_pours);

    brew_session_start(1000, &session);
    mu_assert_int_eq(BrewPhaseRUNNING, session.phase);

    mu_assert("complete in RUNNING must return true", brew_session_complete(&session));
    mu_assert_int_eq(1, session.completed_pours);
    mu_assert_int_eq(25, session.next_pour_sec);
    mu_assert_int_eq(BrewPhaseDONE, session.phase);

    // Повторное завершение в DONE не удваивает счётчик и время.
    mu_assert("complete in DONE must return false", !brew_session_complete(&session));
    mu_assert_int_eq(1, session.completed_pours);
    mu_assert_int_eq(25, session.next_pour_sec);
}

MU_TEST(test_abort_keeps_counters) {
    BrewSession session = brew_make_session(20, 5, 6);
    brew_session_start(0, &session);

    brew_session_abort(&session);
    mu_assert_int_eq(BrewPhaseIDLE, session.phase);
    mu_assert_int_eq(0, session.completed_pours);
    mu_assert_int_eq(20, session.next_pour_sec);
}

MU_TEST(test_over_limit_boundary) {
    BrewSession session = brew_make_session(20, 5, 2);
    brew_session_start(0, &session);

    brew_session_complete(&session);
    mu_assert("1 of 2 is not over", !brew_session_over_limit(&session));
    brew_session_complete(&session);
    mu_assert("2 of 2 is not over", !brew_session_over_limit(&session));
    session.completed_pours = 3;
    mu_assert("3 of 2 is over", brew_session_over_limit(&session));
    session.completed_pours = 1;
    mu_assert("1 of 2 is not over", !brew_session_over_limit(&session));
}

MU_TEST(test_start_sets_deadline) {
    BrewSession session = brew_make_session(1, 0, 6);
    brew_session_start(1234, &session);
    mu_assert_int_eq(BrewPhaseRUNNING, session.phase);
    mu_assert_int_eq(2234, session.deadline_tick);

    // Повторный старт в RUNNING не сдвигает дедлайн.
    brew_session_start(9999, &session);
    mu_assert_int_eq(2234, session.deadline_tick);

    BrewSession big = brew_make_session(BREW_NEXT_POUR_MAX, 0, 6);
    brew_session_start(1000, &big);
    mu_assert_int_eq(BrewPhaseRUNNING, big.phase);
    mu_assert_int_eq(1000 + 63000 * 1000, big.deadline_tick);
}

MU_TEST(test_remaining_sec_rounding) {
    mu_assert_int_eq(1, brew_session_remaining_sec(1000, 1999));
    mu_assert_int_eq(1, brew_session_remaining_sec(0, 1));
    mu_assert_int_eq(20, brew_session_remaining_sec(0, 20000));
    mu_assert_int_eq(0, brew_session_remaining_sec(5000, 5000));
    mu_assert_int_eq(0, brew_session_remaining_sec(6000, 5000));
}

static void run_minunit_test_brew_math_suite(void) {
    MU_RUN_TEST(test_pour_sec_adds_increment);
    MU_RUN_TEST(test_pour_sec_clamps_to_max);
    MU_RUN_TEST(test_format_time);
    MU_RUN_TEST(test_reset_from_preset);
    MU_RUN_TEST(test_reset_keeps_water_temp);
    MU_RUN_TEST(test_complete_increments_only_on_running);
    MU_RUN_TEST(test_abort_keeps_counters);
    MU_RUN_TEST(test_over_limit_boundary);
    MU_RUN_TEST(test_start_sets_deadline);
    MU_RUN_TEST(test_remaining_sec_rounding);
}

int run_minunit_test_brew_math(void) {
    run_minunit_test_brew_math_suite();
    return minunit_status;
}
