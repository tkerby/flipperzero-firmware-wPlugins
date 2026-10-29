---

description: "Список задач реализации фичи Tea Timer — таймер заваривания чая проливами"
---

# Задачи: Tea Timer — таймер заваривания чая проливами

**Входные документы**: `/specs/001-tea-brew-timer/` — `plan.md` (обязателен), `spec.md`
(обязателен, истории US1–US4), `research.md`, `data-model.md`,
`contracts/tea-file-format.md`, `contracts/storage-layout.md`, `contracts/ui-screens.md`,
`quickstart.md`, конституция проекта 3.0.1.

**Тесты**: юнит-тесты **обязательны** (принцип VIII конституции 3.0.1) и исполняются **на
устройстве** через CLI-команду `tea_timer_test`; хостовый прогон зачётным не является.
Ручные сценарии — `quickstart.md` §5 (S1–S5).

**Организация**: задачи сгруппированы по историям пользователя, каждая история —
самостоятельно реализуемый и проверяемый инкремент.

## Формат: `[ID] [P?] [Story] Описание`

- **[P]**: можно выполнять параллельно (разные файлы, нет зависимостей от незавершённых задач)
- **[Story]**: история пользователя (`US1`, `US2`, `US3`, `US4`)
- В описании — точный путь к файлу

## Соглашения о путях

Проект — одно FAP-приложение в корне рабочего каталога; все пути ниже относительны
`/Users/igordanilov/CLionProjects/flipper/tea_timer`. Сборка, стиль и прогон — только
`ufbt` (`ufbt`, `ufbt launch`, `ufbt format`, `ufbt lint`, `ufbt cli`).

Ключевые проверенные факты SDK 1.4.3 (`~/.ufbt/current/sdk_headers/f7_sdk`), на которые
опираются задачи:

- `printf`, `puts`, `vsnprintf` **не экспортируются в FAP** (`api_symbols.csv` — `-`);
  для вывода отчёта тестов использовать `snprintf` (`+`) в буфер + `pipe_send(pipe, ...)`
  (`+`), где `pipe` — первый аргумент `CliCommandExecuteCallback`;
- CLI: `cli_registry_add_command_ex(registry, name, flags, cb, ctx, stack_size)` и
  `cli_registry_delete_command(registry, name)`; `<cli/cli.h>` — совместимый заголовок,
  `RECORD_CLI` определён в нём; `CliCommandFlagParallelSafe` есть;
- `file_browser_configure(browser, extension, base_path, skip_assets, hide_dot_files,
  file_icon, hide_ext)`, `elements_string_fit_width(Canvas*, FuriString*, size_t)`,
  `widget_add_button_element(Widget*, GuiButtonType, const char*, ButtonCallback, void*)`,
  `dialog_message_show_storage_error(DialogsApp*, const char*)` — сигнатуры подтверждены;
- `memmgr_get_free_heap()` экспортируется — используется для строки `Leaked:`.

---

## Phase 1: Setup (общая инфраструктура)

**Цель**: создать скелет FAP-проекта и общие типы, от которых зависят все истории.

- [X] T001 Создать `application.fam`: `appid="tea_timer"`, `name="Tea Timer"`,
      `apptype=FlipperAppType.EXTERNAL`, `entry_point="tea_timer_app"`,
      `requires=["gui", "storage", "notification", "dialogs", "cli"]`, `stack_size=2*1024`,
      `fap_version="1.0"` (строкой), `fap_category="Tools"`, `fap_icon="images/10px.png"`,
      `fap_icon_assets="images"` (генерирует `tea_timer_icons.h` с `I_tea_file`),
      `fap_author`, `fap_description="Tea brewing timer with pour-over presets"` (принцип I)
- [X] T002 [P] Добавить иконки в `images/`: `10px.png` (fap_icon) и `tea_file_10px.png`
      (иконка `.tea`-файла для `file_browser`) — 10×10 px, 1 бит/пиксель, монохромные;
      обе попадают в `fap_icon_assets`
- [X] T003 [P] Создать `tea_timer.h`: `#define TEA_NAME_MAX 24`, `#define TEA_PATH_MAX 128`,
      `#define TEA_PARSE_BUF_SIZE 1025` (1024 байта файла + `\0`),
      `enum TeaPresetResult` (`TeaPresetResultOK`, `...ERR_NOT_FOUND`, `...ERR_TOO_LARGE`,
      `...ERR_NO_FIELD`, `...ERR_BAD_NUMBER`, `...ERR_OUT_OF_RANGE`,
      `...ERR_DUPLICATE_FIELD`, `...ERR_EMPTY_NAME`, `...ERR_BAD_NAME`), объявление
      `typedef struct PresetError { TeaPresetResult result; char error_field[16];
      char file_name[32]; }` (`data-model.md` §5), `typedef struct TeaPreset`
      с полями `char name[TEA_NAME_MAX+1]` (1–24 байта, латиница/цифры/пробел/`_`),
      `uint16_t leaf_tenths_g` (1–500), `uint16_t first_pour_sec` (1–3600),
      `uint16_t increment_sec` (0–600), `uint8_t max_pours` (1–99), `char path[TEA_PATH_MAX]`;
      объявление `tea_timer_app(void* p)`
- [X] T004 [P] Создать `tea_timer_custom_event.h`: `enum TeaTimerCustomEvent` —
      `TeaTimerEventFileSelected`, `TeaTimerEventReloadPressed`, `TeaTimerEventBrewStart`,
      `TeaTimerEventBrewStop`, `TeaTimerEventTick` (тик 1 Гц, пересылается из `tea_timer.c`);
      `MenuSelected` / `SkippedClosed` не объявляются — потребителей нет
      (`data-model.md` §4, `contracts/ui-screens.md` C-UI-0…C-UI-7)
- [X] T005 Создать `tea_timer_i.h`: `enum TeaTimerViewId`, `typedef struct TeaTimerApp` с
      полями `Gui*`, `NotificationApp*`, `Storage*`, `DialogsApp*`, `ViewDispatcher*`,
      `SceneManager*`, `FuriString* file_path`, `Submenu*`, `FileBrowser*`, `Widget*`
      (карточка: IDLE + Start), `Widget*` (нет пресетов), `View*` (brew_view),
      `BrewSession session`, `char* parse_buf` (буфер разбора 1025 байт **в куче**);
      комментарии — на русском (принципы II, V). Поля `session_dirty` нет — не используется
- [X] T006 Проверить фактический флаг отладочной сборки в `application.fam`: выполнить `ufbt VERBOSE=1` и
      убедиться, определяется ли `FURI_DEBUG`; если нет — добавить
      `cdefines=["TEA_TIMER_TESTS"]` в `application.fam`, а в коде регистрировать тесты под
      `#if defined(FURI_DEBUG) || defined(TEA_TIMER_TESTS)` (research.md R-B-3)

---

## Phase 2: Foundational (блокирующие предпосылки)

**Цель**: чистая логика (`lib/`), каркас юнит-тестов на устройстве, каркас UI и жизненный
цикл приложения.

**⚠️ КРИТИЧНО**: ни одна история не начинается, пока эта фаза не завершена.

- [X] T007 [P] Создать `lib/brew_math.h`: `typedef enum { BrewPhaseIDLE, BrewPhaseRUNNING,
      BrewPhaseDONE } BrewPhase`; `typedef struct BrewSession` с `bool has_preset`,
      `TeaPreset preset`, `uint16_t completed_pours`, `uint16_t next_pour_sec`,
      `BrewPhase phase`, `uint32_t deadline_tick`; константа `BREW_NEXT_POUR_MAX 63000`;
      прототипы `brew_session_reset_from_preset(TeaPreset*, BrewSession*)`,
      `brew_math_pour_sec`, `brew_session_start(uint32_t now_tick, BrewSession*)`,
      `brew_session_remaining_sec(uint32_t now_tick, uint32_t deadline_tick)`,
      `brew_session_complete`, `brew_session_abort`, `brew_session_over_limit`,
      `brew_math_format_time` (без зависимостей от Furi)
- [X] T008 [P] Реализовать `lib/brew_math.c` (чистая логика, только libc с границами):
      `brew_math_pour_sec(prev, increment)` = `prev + increment` с ограничением
      `BREW_NEXT_POUR_MAX`; `brew_session_complete()` — **единственное** место, где
      `completed_pours++`, а `next_pour_sec` вычисляется только через
      `brew_math_pour_sec(next_pour_sec, preset.increment_sec)` (одна точка правды для
      правила приращения и ограничения), переход `RUNNING → DONE`, возврат `true` только
      при фактическом завершении;
      `brew_session_abort()` — `RUNNING → IDLE` без изменения счётчика и времени (FR-012,
      FR-015); `brew_session_over_limit()` = `completed_pours > preset.max_pours` (FR-014);
      `brew_math_format_time(sec, buf, size)` — `mm:ss` при `sec < 3600`, иначе `hh:mm:ss`
      (FR-018); `brew_session_remaining_sec(now_tick, deadline_tick)` =
      `(deadline_tick > now_tick) ? (deadline_tick - now_tick + 999) / 1000 : 0` — без
      обращения к `furi_get_tick()` внутри, время передаётся параметром;
      `brew_session_start(now_tick, session)` — `deadline_tick = now_tick +
      next_pour_sec * 1000`, `phase = RUNNING`, повторный вызов в `RUNNING` игнорируется,
      без `furi_get_tick()` внутри
- [X] T009 [P] Создать `lib/tea_preset.h`: прототип
      `TeaPresetResult tea_preset_parse_buf(const char* buf, size_t len, TeaPreset* out,
      char* error_field, size_t error_field_size)` (слой без Storage)
- [X] T010 Реализовать `lib/tea_preset.c` по алгоритму
      `contracts/tea-file-format.md` (шаги 2, 4–11): снятие BOM `EF BB BF`, разбиение по
      `\n`, обрезка `\r` и пробелов/табов, пропуск пустых строк и строк с `#`, разделение по
      первому `:`, пять обязательных ключей ровно по одному разу
      (`ERR_DUPLICATE_FIELD` / `ERR_NO_FIELD` с именем первого отсутствующего в
      `error_field`), числа через `strtoul` с проверкой `endptr` до конца значения
      (`20abc` → `ERR_BAD_NUMBER`), `leaf_g_per_100ml` — точка обязательна и ровно одна,
      после неё ровно одна цифра (`5`, `5.`, `5.00`, `.5` → `ERR_BAD_NUMBER`), хранение в
      десятых долях (`0.1`→`1`, `50.0`→`500`), пустой `name` →
      `ERR_EMPTY_NAME`, недопустимые символы `name` (не латиница/цифры/пробел/`_`) →
      `ERR_BAD_NAME`, `name` длиннее 24 байт → `ERR_OUT_OF_RANGE`, все значения вне
      диапазонов FR-004 → `ERR_OUT_OF_RANGE`; неизвестные ключи игнорируются
- [X] T011 [P] Вендорить `unit_tests/minunit.h`: `MU_TEST`, `MU_RUN_TEST`, `MU_RUN_SUITE`,
      `MU_REPORT`, `mu_assert`, `mu_assert_int_eq`, `mu_assert_string_eq`,
      `mu_assert_mem_eq`, `MINUNIT_MESSAGE_LEN 256`; вывод (`MU_PRINT`/`minunit_print_fail`)
      — через `snprintf` + `pipe_send`, без `printf`/`puts` (не экспортируются в FAP)
- [X] T012 [P] Создать `unit_tests/minunit_vars.h`: глобальные `minunit_run`,
      `minunit_assert`, `minunit_fail`, `minunit_status`, `minunit_last_message[256]`
- [X] T013 Создать `unit_tests/test_index.h` и `unit_tests/test_index.c`: реестр
      `typedef struct { const char* name; int (*entry)(void); } UnitTest;` с наборами
      `tea_preset` и `brew_math`; `tea_timer_tests_register()` открывает `RECORD_CLI` и
      вызывает `cli_registry_add_command_ex(registry, "tea_timer_test",
      CliCommandFlagParallelSafe, cb, NULL, 2048)`; `tea_timer_tests_unregister()` —
      `cli_registry_delete_command` + `furi_record_close`; callback принимает
      `PipeSide* pipe` и `FuriString* args` (необязательное имя набора), печатает отчёт:
      строки тестов, `MU_REPORT()`, `Time: <ms> ms` (разница `furi_get_tick()` до и после
      прогона: `(uint32_t)(stop - start)`), `Leaked: <delta>` по `memmgr_get_free_heap()`
      до/после, `Status: PASSED|FAILED`, и отправляет `sequence_success` / `sequence_error`
      (принцип VIII, research.md R-B-2/R-B-3)
- [X] T014 [P] Создать `unit_tests/lib_tests/brew_math/brew_math_test.c` —
      `run_minunit_test_brew_math()`: `next = prev + increment`, отсутствие роста при
      `increment_sec = 0`, ограничение `63000`, форматирование `00:20` / `01:00:00` /
      `17:30:00`, `over_limit` при `completed > max`, инкремент счётчика **только** в
      `brew_session_complete()`, отсутствие инкремента после `brew_session_abort()`,
      повторный `complete()` в `DONE` не удваивает счётчик; `brew_session_start(now, s)` →
      `deadline_tick = now + sec*1000` (проверить `sec = 1` и `sec = 63000`, без
      переполнения `uint32_t`), повторный `start()` в `RUNNING` не сдвигает дедлайн;
      `brew_session_remaining_sec()` — округление вверх (`999 мс → 1`), `0` при
      `now == deadline`, `0` при `now > deadline`; `brew_session_reset_from_preset()` —
      `completed_pours = 0`, `next_pour_sec = first_pour_sec`, `phase = IDLE`,
      `has_preset = true`; `over_limit` на границе (`completed == max` → `false`,
      `completed == max + 1` → `true`) (покрывает US1, US2, US4; принцип VIII)
- [X] T015 [P] Создать `unit_tests/lib_tests/tea_preset/tea_preset_test.c` —
      `run_minunit_test_tea_preset()`: эталонный файл из
      `contracts/tea-file-format.md` (`leaf_tenths_g = 50`), `\r\n`, BOM, комментарии и
      пустые строки, границы `0.1`/`50.0`, `1`/`3600`, `0`/`600`, `1`/`99`,
      запись без точки `5` и с лишней цифрой `5.00` → `ERR_BAD_NUMBER`, вне границ
      (`0`, `50.1`, `0`, `3601`, `601`, `0`, `100`), `20abc` → `ERR_BAD_NUMBER`, отсутствие
      `max_pours` → `ERR_NO_FIELD` + `error_field = "max_pours"`, пустой `name` →
      `ERR_EMPTY_NAME`, дубликат `name` → `ERR_DUPLICATE_FIELD`, неизвестный ключ `temp_c`
      игнорируется, `name` длиннее 24 байт → `ERR_OUT_OF_RANGE`, `name: Green-Tea` →
      `ERR_BAD_NAME` (покрывает US1, US3)
- [X] T016 Создать `views/brew_view.h`: `typedef struct BrewViewModel` из `data-model.md` §4
      (`phase`, `char name[25]`, `completed_pours`, `max_pours`, `over_limit`,
      `leaf_tenths_g`, `pour_number`, `pour_sec`, `remaining_sec`); прототипы
      `brew_view_alloc()`, `brew_view_free(BrewView*)`, `brew_view_get_view(BrewView*)`,
      `brew_view_update(BrewView*, const BrewSession*)`
- [X] T017 Создать `views/brew_view.c` — каркас: `view_alloc()` +
      `view_allocate_model(view, ViewModelTypeLockFree, sizeof(BrewViewModel))`,
      `view_set_context(app)`, `view_set_draw_callback`, `view_set_input_callback`: на
      `InputTypeShort` + `InputKeyOk` view **не меняет сессию**, а пересылает
      `view_dispatcher_send_custom_event(vd, phase == BrewPhaseRUNNING ?
      TeaTimerEventBrewStop : TeaTimerEventBrewStart)`; модель
      обновляется только через `with_view_model(..., true)` из GUI-потока;
      `brew_view_update()` копирует поля сессии и форматирует время через
      `brew_math_format_time()`; в `draw_callback` — только чтение модели и отрисовка
      подсказок кнопок; без `alloc` внутри draw (принцип V)
- [X] T018 Создать `scenes/tea_timer_scene_config.h` (макрос со списком сцен) и
      `scenes/tea_timer_scene.h` (`enum TeaTimerScene`, объявления
      `tea_timer_scene_<name>_on_enter/on_event/on_exit`), `scenes/tea_timer_scene.c`
      (массив `tea_timer_scene_handlers[]`; на этой фазе — только `Menu`)
- [X] T019 Создать `scenes/tea_timer_scene_menu.c` — `submenu`: `submenu_reset`,
      `submenu_set_header(sm, "Tea Timer")`, `submenu_add_item(sm, "Presets", 0, cb, app)`;
      пункт `New preset` и переходы в 002 добавляются отдельной фичей; BACK в корневой сцене
      → `view_dispatcher_stop` (C-UI-1, FR-023)
- [X] T020 Создать `tea_timer.c` — жизненный цикл и владение ресурсами (порядок из
      `plan.md`, раздел «Порядок владения ресурсами»): `furi_record_open` × 4, парные
      `*_alloc`, `view_dispatcher_alloc`, `scene_manager_alloc`, `view_dispatcher_set_custom_event_callback`,
      `view_dispatcher_set_navigation_event_callback`,
      `view_dispatcher_set_tick_event_callback(vd, tea_timer_tick_callback, 1000)`,
      `view_dispatcher_add_view` для `Menu` и `TeaTimerViewBrew`, attach
      `ViewDispatcherTypeFullscreen`, `scene_manager_next_scene(TeaTimerSceneMenu)`;
      `tea_timer_free()` — строго обратный порядок, `tea_timer_tests_unregister()` под
      `#if defined(FURI_DEBUG) || defined(TEA_TIMER_TESTS)`; тик-callback пересылает
      `TeaTimerEventTick` в активную сцену через `view_dispatcher_send_custom_event`
- [X] T021 Проверить каркас на устройстве (`tea_timer.c`, `application.fam`): `ufbt` (без предупреждений), `ufbt launch`,
      BACK корректно выходит; затем `ufbt cli` → `tea_timer_test` печатает отчёт
      `Status: PASSED`, `Leaked: 0` (проверить также `unit_tests/test_index.c`)

**Checkpoint**: каркас собирается и запускается, юнит-тесты логики проходят на устройстве —
реализация историй может начинаться.

---

## Phase 3: User Story 1 — выбор пресета и начало заваривания (P1) 🎯 MVP

**Цель**: пользователь выбирает `.tea` через системный браузер файлов, видит карточку,
запускает пролив, получает однократный сигнал; счётчик и время следующего пролива растут
по правилу приращения.

**Independent Test**: `quickstart.md` §5 S1 — положить 2–3 `.tea` в
`/ext/apps_data/tea_timer/`, открыть приложение, выбрать пресет, `Start`, дождаться сигнала,
проверить `Pours 1 / 6` и `Pour 2 00:25`.

- [X] T022 [P] [US1] Создать `lib/tea_preset_load.h` и `lib/tea_preset_load.c` — слой
      Storage над `tea_preset_parse_buf()`; прототип:
      `TeaPresetResult tea_preset_load(Storage* storage, const char* path, char* buf,
      size_t buf_size, TeaPreset* out, PresetError* err)` (без зависимости от
      `TeaTimerApp`; буфер разбора `app->parse_buf` передаётся вызывающей сценой):
      `storage_file_open(path, FSAM_READ, FSOM_OPEN_EXISTING)` → `ERR_NOT_FOUND`;
      `storage_file_size() > 1024` → `ERR_TOO_LARGE` без чтения; чтение в `buf` (1025 байт,
      в куче) + `\0`; вызов парсера; `storage_file_close()` **всегда**, на всех ветках
      (contracts/tea-file-format.md, storage-layout.md)
- [X] T023 [P] [US1] Создать `scenes/tea_timer_scene_file_select.c` — `file_browser`:
      `#include "tea_timer_icons.h"`; `furi_string_alloc_set(APP_DATA_PATH(""))` в `alloc`,
      `file_browser_alloc(result_path)`,
      `file_browser_configure(fb, ".tea", APP_DATA_PATH(""), false, true, &I_tea_file, true)`,
      `file_browser_set_callback` → `view_dispatcher_send_custom_event(vd,
      TeaTimerEventFileSelected)`; `on_enter` — `file_browser_start` с корнем
      `APP_DATA_PATH("")`, `on_exit` — `file_browser_stop`; BACK в корне → `SceneMenu`;
      лог `tea_timer: file select at /ext/apps_data/tea_timer` (C-UI-2, FR-002)
- [X] T024 [P] [US1] Создать `scenes/tea_timer_scene_preset_card.c` — `widget`: название
      (`FontPrimary`, центр), `Leaf N.N g/100ml`, `Pours X / N`, `Pour N  mm:ss`,
      `widget_add_button_element(w, GuiButtonTypeCenter, "Start", cb, app)`; BACK →
      `SceneFileSelect`; карточка и есть экран заваривания в ожидании (`IDLE`), отдельного
      промежуточного экрана нет; `Start` → переход в `SceneBrew`, сам отсчёт стартует по
      `TeaTimerEventBrewStart` (T026); создание и сброс сессии — только в T024a/T024b
      (C-UI-3, FR-009/FR-010, SC-001)
- [X] T024a [US1] Обработать выбор файла в `scenes/tea_timer_scene_file_select.c`
      (основной путь US1, FR-001/FR-003): Custom `TeaTimerEventFileSelected` →
      `tea_preset_load(app->storage, furi_string_get_cstr(app->file_path), app->parse_buf,
      TEA_PARSE_BUF_SIZE, &app->session.preset, &err)`; при `OK` — сброс/создание сессии
      (T024b) и `scene_manager_next_scene(sm, TeaTimerScenePresetCard)`; при `!= OK` —
      диалог C-UI-10 из T038, остаёмся в `SceneFileSelect`. Зависит от T010, T022, T024 —
      метку `[P]` не ставить; ветку ошибки (T038) не дублировать, вызывать её функцию
- [X] T024b [US1] Реализовать правило сессии (FR-016, Clarifications 2026-10-28): серия
      сохраняется между открытиями одного файла и сбрасывается при выборе другого.
      В обработчике из T024a — если `!app->session.has_preset` **или**
      `strcmp(app->file_path, app->session.preset.path) != 0`, вызвать
      `brew_session_reset_from_preset(&app->session.preset, &app->session)`; тот же путь —
      `session.preset.path` заполняется из выбранного `file_path` (обрезка до
      `TEA_PATH_MAX - 1`). При совпадении пути серия продолжается без сброса; отсчёт не
      возобновляется автоматически
- [X] T025 [US1] Расширить `views/brew_view.c` раскладками: `RUNNING` — остаток шрифтом
      `FontBigNumbers` по центру, `Pours X / N`, подсказка `OK: stop`;
      `DONE` — инверсный баннер `Time is up!`, `Pours X / N`, `Pour N+1  mm:ss`,
      подсказка `OK: start next pour`; состояние `IDLE` в `brew_view` не рисуется —
      ожидание показывает карточка (T024); обрезка длинного названия —
      `elements_string_fit_width(canvas, furi_string, width)` (C-UI-4, FR-009, FR-013)
- [X] T026 [US1] Создать `scenes/tea_timer_scene_brew.c`: `on_enter` — `brew_view_update`
      из сессии + `view_dispatcher_switch_to_view(vd, TeaTimerViewBrew)` (состояние
      `RUNNING` — продолжение отсчёта, `DONE` — показ завершения); `on_event`:
      Custom `TeaTimerEventTick` — пересчёт `remaining_sec` по `deadline_tick`/`furi_get_tick()`,
      при нуле `brew_session_complete()` + сигнал + `brew_view_update`
      (`with_view_model(..., true)` перерисовывает только при смене секунды);
      Custom `TeaTimerEventBrewStart` — `brew_session_start(furi_get_tick(), &app->session)` +
      `sequence_display_backlight_enforce_on` (повторный старт в `RUNNING` игнорируется);
      Custom `TeaTimerEventBrewStop` — `brew_session_abort()` +
      `scene_manager_previous_scene()` (на карточку; счётчик и время пролива не меняются);
      BACK — то же, что `BrewStop`; логи `tea_timer: pour <n> start <sec>s` / `abort` /
      `done <sec>s` (FR-011…FR-016, R-T-1)
- [X] T027 [US1] Добавить сигнал завершения в `scenes/tea_timer_scene_brew.c`: `static const NotificationSequence
      tea_timer_sequence_done` (две `message_note_*` + `message_vibro_on`/`_off` +
      `message_delay_*`) и `tea_timer_sequence_done_reset` (`sequence_reset_sound`,
      `sequence_reset_vibro`); отправка ровно один раз на переходе `RUNNING → DONE` через
      `notification_message(app->notification, ...)`
      (R-T-2, FR-019)
- [X] T028 [US1] Зарегистрировать новые сцены и переходы в `scenes/tea_timer_scene_config.h`: добавить `TeaTimerSceneFileSelect`,
      `TeaTimerScenePresetCard`, `TeaTimerSceneBrew` в `scenes/tea_timer_scene.c`; в
      `scenes/tea_timer_scene_menu.c` — обработка
      `Presets` → `SceneFileSelect`; в `tea_timer.c` — `view_dispatcher_add_view` /
      `remove_view` для `file_browser` и `widget` карточки (парность, принцип V)
- [X] T029 [US1] Дополнить `unit_tests/lib_tests/tea_preset/tea_preset_test.c` тестами
      `tea_preset_load()`: создать `APP_DATA_PATH("test")` через `Storage`, записать файл,
      прочитать (эталон → `OK`), проверить границу 1024/1025 байт (`ERR_TOO_LARGE`),
      несуществующий путь (`ERR_NOT_FOUND`), удалить файл и каталог в конце теста
      (принцип VIII: тест готовит и убирает данные сам)
- [X] T030 [US1] Проверить US1: `ufbt`, `ufbt launch`, сценарий S1 из `quickstart.md`
      (включая `tea_timer_test` → `Status: PASSED`, `Leaked: 0`) — правки по результатам
      вносятся в `scenes/tea_timer_scene_brew.c` и `views/brew_view.c`

**Checkpoint**: US1 полностью работоспособна и проверяема отдельно (MVP).

---

## Phase 4: User Story 2 — продолжение серии сверх рекомендации (P2)

**Цель**: превышение `max_pours` не блокирует работу: счётчик показывает `9 / 8` и плашку
`OVER`, проливы продолжаются с растущим временем.

**Independent Test**: `quickstart.md` §5 S2 — пресет с `max_pours: 2`, три пролива подряд,
третий запускается без диалогов, экран показывает `Pours 3 / 2` + `OVER`.

- [X] T031 [US2] Реализовать признак превышения в `views/brew_view.c`: при
      `view_model->over_limit` счётчик `Pours X / N` рисуется инверсным блоком
      (`canvas_draw_rbox` + белый текст) и добавляется плашка `OVER`; проверка, что счётчик и
      строка `Leaf` остаются на экране при длинном названии (обрезка по ширине)
      (C-UI-4, FR-014, Edge Cases)
- [X] T032 [US2] Передавать `over_limit` в модель в `scenes/tea_timer_scene_brew.c` и не создавать блокирующих диалогов: заполнять `view_model->over_limit =
      brew_session_over_limit(&app->session)` при каждом `Tick`, старте и завершении;
      запуск пролива при `completed_pours >= max_pours` идёт тем же путём, что обычный,
      без `DialogMessage`/`popup` (FR-014, SC-005)
- [X] T033 [US2] Проверить US2 (`views/brew_view.c`, `scenes/tea_timer_scene_brew.c`): положить `.tea` с `max_pours: 2` (например,
      `specs/001-tea-brew-timer/examples/`), выполнить S2; зафиксировать, что 4-й пролив
      стартует и время растёт по правилу приращения
      — **проверено на устройстве** (`quick_two_pours.tea`, `max_pours: 2`): проливы 5 → 7 → 9 с,
      после третьего экран показал `Pours 3 / 2` + плашку `OVER`, 4-й пролив стартовал
      на `00:11` без диалогов; счётчик и плашка совпадают с макетом C-UI-4

**Checkpoint**: US1 и US2 работают независимо.

---

## Phase 5: User Story 3 — устойчивость к содержимому SD-карты (P2)

**Цель**: некорректные файлы, отсутствие каталога и вынутая карта не роняют приложение и
дают понятное английское сообщение.

**Independent Test**: `quickstart.md` §5 S3 и S4 — открыть заведомо некорректный `.tea`
(диалог с причиной, возврат в браузер), переименовать каталог (`No presets found` + `Reload`),
извлечь карту (`SD card not found`, OK — повтор, BACK — выход).

- [X] T034 [P] [US3] Создать `scenes/tea_timer_scene_no_presets.c` — `widget`:
      `No presets found`, `Add .tea files to`, `apps_data/tea_timer`,
      `widget_add_button_element(w, GuiButtonTypeCenter, "Reload", cb, app)`; OK/`Reload` —
      повторная проверка каталога и при появлении файлов переход в `SceneFileSelect`; BACK —
      в меню (C-UI-5, FR-008). Там же реализовать `bool tea_timer_has_presets(Storage*)`:
      `storage_dir_open` + цикл `storage_dir_read` по `APP_DATA_PATH("")`, пропуск каталогов
      (`file_info_is_dir`), dot-файлов и файлов без расширения `.tea`; `storage_dir_close()`
      на всех ветках, включая неудачное открытие (принцип V, storage-layout.md:64)
- [X] T035 [P] [US3] Создать `scenes/tea_timer_scene_error.c` — состояние «SD-карта
      недоступна»: короткий английский текст `SD card not found`, подсказка
      `Insert the card and press OK`, OK — повторная проверка `storage_sd_status`, BACK —
      выход; фоновая проверка перед входом в `SceneFileSelect` использует
      `dialog_message_show_storage_error()` из `dialogs` (C-UI-6, FR-024)
- [x] ~~T036~~ Удалена по результатам анализа: сцена `Skipped` и счётчик пропущенных
      файлов не нужны после замены FR-006 на `file_browser` (FR-007 переформулирован,
      причина показывается диалогом C-UI-10 в T038). Сцена остаётся только в
      `contracts/ui-screens.md` C-UI-7 как резерв для 002.
- [X] T037 [US3] Добавить проверку носителя и каталога в `scenes/tea_timer_scene_file_select.c`
      перед показом браузера: `storage_sd_status() != StorageStatusOK` → `SceneError`;
      `!storage_dir_exists(APP_DATA_PATH(""))` → `SceneNoPresets`;
      `!tea_timer_has_presets(app->storage)` (T034) → `SceneNoPresets` — пустой каталог
      даёт `No presets found`, как требует FR-008 и C-UI-5; скан выполняется только по
      корню `APP_DATA_PATH("")` и только на факт наличия `.tea`;
      лог `tea_timer: sd status <code>` (storage-layout.md, матрица состояний)
- [X] T038 [US3] Реализовать диалог ошибки выбранного файла в `scenes/tea_timer_scene_file_select.c`: при `TeaPresetResult != OK` —
      `dialog_message_alloc()`, `dialog_message_set_header(msg, "Cannot use file", …)`,
      текст из таблицы `data-model.md` §5 (`Missing field: %s`, `Not a number: %s`,
      `Out of range: %s`, `Duplicate field: %s`, `Name is empty`, `Bad name: %s`,
      `File is larger than 1 KB`, `Cannot open file`),
      `dialog_message_set_buttons(msg, "Retry", NULL, "OK")`,
      `dialog_message_show()`; после закрытия — остаёмся в `SceneFileSelect`, файл остаётся
      виден; `dialog_message_free` на всех ветках; лог
      `tea_timer: preset error <code> field=<field>` (C-UI-10, FR-003/FR-007)
- [X] T039 [US3] Зарегистрировать сцены в `scenes/tea_timer_scene_config.h`:
      `TeaTimerSceneNoPresets`, `TeaTimerSceneError` — а также в `scenes/tea_timer_scene.c`,
      добавить `view_dispatcher_add_view`/`remove_view` для `widget` «нет пресетов» в
      `tea_timer.c` (парность ресурсов). `popup` заводится только вместе с 002
      («Сохранено: file.tea»), в 001 не нужен
- [X] T040 [US3] Проверить US3: сценарии S3 и S4 из `quickstart.md` на устройстве
      (некорректный файл из `examples/`, отсутствие каталога, извлечение карты; приложение не
      падает, список остаётся отзывчивым) — правки в `scenes/tea_timer_scene_file_select.c`
      — **проверено на устройстве**: `broken_example.tea` → диалог `Cannot use file` /
      `Not a number: leaf_g_per_100ml`, OK возвращает в браузер без падения; каталог,
      переименованный в `tea_timer_off`, даёт `No presets found` с `Reload`, после возврата
      каталога `Reload` открывает список; извлечение SD-карты и `SD card not found`
      (OK — повтор, BACK — выход) проверены вручную. По ходу проверки найден и исправлен баг:
      `APP_DATA_PATH("")` даёт путь `/data/` с завершающим слэшем, из-за чего
      `storage_dir_open()`/`storage_dir_exists()` возвращали `FSE_INVALID_NAME` и файловый
      браузер не открывался; теперь используется `STORAGE_APP_DATA_PATH_PREFIX` (`/data`)

**Checkpoint**: US1, US2 и US3 работают независимо.

---

## Phase 6: User Story 4 — точный и предсказуемый отсчёт (P3)

**Цель**: сигнал звучит вовремя и ровно один раз, экран не гаснет во время отсчёта,
прерванный пролив не портит счётчик, выход из приложения не оставляет звук и подсветку.

**Independent Test**: `quickstart.md` §5 S5 — пресет с `first_pour_sec: 60`, секундомер
рядом: отклонение ≤ 1 с, экран не гаснет, OK во время отсчёта останавливает пролив без
изменения `Pours`, повторный запуск — снова 60 с.

- [X] T041 [US4] Управлять подсветкой в `scenes/tea_timer_scene_brew.c`:
      `sequence_display_backlight_enforce_on` при старте пролива (`IDLE → RUNNING`,
      `DONE → RUNNING`), `sequence_display_backlight_enforce_auto` при
      `RUNNING → DONE`, `RUNNING → IDLE` (STOP) и на `on_exit` сцены; гарантировать
      возврат в авто на всех путях (R-T-3, FR-017, SC-006)
- [X] T042 [US4] Обеспечить однократность сигнала в `scenes/tea_timer_scene_brew.c` и отсутствие автовозврата: переход `RUNNING → DONE` выполняется ровно один раз
      (проверка `phase == BrewPhaseRUNNING` перед `brew_session_complete()`), в `DONE` тик
      только перерисовывает, сигнал не повторяется, автозапуска следующего пролива нет;
      `DONE` сохраняется до OK/BACK (FR-013, FR-019, SC-002)
- [X] T043 [US4] Корректно завершать работу во время отсчёта в `tea_timer.c`: в
      `tea_timer_free()` при активной сессии — `brew_session_abort()`, отправка
      `sequence_display_backlight_enforce_auto` и `sequence_reset_sound` /
      `sequence_reset_vibro`, затем обычный обратный порядок освобождения; звук, вибро и
      принудительная подсветка после выхода не остаются (FR-023, Edge Cases «Уход из
      приложения во время отсчёта»)
- [X] T044 [US4] Проверить US4 (`scenes/tea_timer_scene_brew.c`, `lib/brew_math.c`):
      сценарий S5 с секундомером на устройстве и сверка вёрстки с макетами
      `specs/001-tea-brew-timer/ui/`; при отклонении > 1 с — проверить
      `brew_session_start()` (расчёт `deadline_tick`) и `brew_session_remaining_sec()` в
      `lib/brew_math.c`
      — **проверено на устройстве** (`accuracy_60.tea`, `first_pour_sec: 60`): отсчёт
      завершился через 60.21 с по внешнему таймеру (включая задержку RPC), отклонение ≤ 1 с;
      экран не гаснет во время отсчёта, после завершения и выхода подсветка возвращается
      в авто (проверено вручную); OK во время отсчёта останавливает пролив — `Pours 0 / 6`
      не изменился, повторный старт снова `01:00`; вёрстка RUNNING/DONE совпадает с макетами
      (счётчик, `Time is up!`, `Pour 2 01:05`, `OK: start`)

**Checkpoint**: все четыре истории независимо работоспособны.

---

## Phase 7: Polish и сквозные требования

**Цель**: документация, стиль, память, итоговый прогон тестов и чек-лист готовности.

- [X] T045 [P] Обновить `README.md` в корне (единственный англоязычный документ, принцип
      VII): установка FAP, каталог `/ext/apps_data/tea_timer/`, формат `.tea` с примером,
      управление, сборка и прогон тестов `ufbt cli` → `tea_timer_test`
      — **создан** `README.md`: требования, установка (ufbt/копирование
      в `/ext/apps/Tools/`), каталог пресетов, таблица полей `.tea` и пример
      `green_tea.tea`, управление по экранам, сборка, прогон тестов на устройстве с
      фактическим отчётом (`20 tests, 101 assertions`)
- [X] T046 [P] Проверить строки интерфейса во всех собственных экранах: `views/brew_view.c`,
      `scenes/tea_timer_scene_menu.c`, `scenes/tea_timer_scene_preset_card.c`,
      `scenes/tea_timer_scene_no_presets.c`, `scenes/tea_timer_scene_error.c`,
      `scenes/tea_timer_scene_file_select.c`, `scenes/tea_timer_scene_brew.c` — только
      английский ASCII, литералы помещаются в 128×64, кириллицы нет; для `file_browser`
      проверить, что подсказки кнопок рисует сам модуль (FR-021, FR-022, конституция)
      — **проверено**: скрипт по всем `*.c/*.h` не нашёл ни одного не-ASCII литерала
      (кириллица только в комментариях), самый длинный литерал — `File is larger than 1 KB`
      (24 символа) в диалоге C-UI-10, который переносит текст по словам; строки `widget`
      короче (≤ 19 символов) и совпадают с макетами, подсказки `file_browser` рисует модуль
- [X] T047 Ревью парности ресурсов и неблокирующего GUI-потока по принципу V в `tea_timer.c`: каждому `*_alloc` — `*_free`, каждому `view_dispatcher_add_view` —
      `remove_view`, каждому `furi_record_open` — `furi_record_close`, ни одного
      `furi_delay_ms`/блокирующего I/O в draw/input/tick-callback-ах; исправить найденное
      — **исправлено**: 6 `add_view` ↔ 6 `remove_view`, порядок снятия приведён к строго
      обратному регистрации; добавлены `furi_check` на `malloc(TeaTimerApp)` и
      `malloc(parse_buf)`; все `furi_record_open` закрываются (CLI — внутри
      `tea_timer_tests_unregister`), `file_browser_start` парен `file_browser_stop` через
      флаг `file_browser_started`; `furi_delay_ms` и блокирующего I/O в draw/input/tick
      нет, `dialog_message_show` — модальный диалог из обработчика события сцены (C-UI-8),
      `storage_file_read` читает не более 1 КБ
- [X] T048 Проверить, что чистый слой `lib/` не тянет Furi и GUI: в `lib/brew_math.h`,
      `lib/brew_math.c`, `lib/tea_preset.h`, `lib/tea_preset.c` нет включений
      `furi.h`/`gui/*` (кроме типов в комментариях), используются только `strtoul`,
      `strlen`, `memcpy`, `snprintf` с границами; `lib/tea_preset_load.c` — единственный
      файл `lib/`, которому разрешены `storage/*` и `furi.h` (принцип II, `plan.md` R-P-1)
      — **проверено** (`grep '#include'`): `brew_math.*` и `tea_preset.*` включают только
      `stdint.h`, `stdbool.h`, `stddef.h`, `stdio.h`, `string.h`, `stdlib.h` и
      `tea_timer.h`; `furi.h` и `storage/*` есть только в `tea_preset_load.*`
- [X] T049 Прогнать `ufbt format` и `ufbt lint` для `tea_timer.c` и каталогов `lib/`,
      `views/`, `scenes/`, `unit_tests/`: повторный `ufbt format` не даёт диффа, `ufbt lint`
      без неразобранных замечаний, сборка без предупреждений компилятора (принцип IV)
      — **пройдено**: первый `ufbt format` переформатировал
      `scenes/tea_timer_scene_error.c`, `scenes/tea_timer_scene_no_presets.c` и
      `tea_timer.c`, повторный запуск диффа не даёт; `ufbt lint` замечаний по коду не
      имеет (только предупреждение линтера об именах каталогов `specs/001-*`, `specs/002-*`,
      которые заданы Spec Kit и не относятся к C-коду); `ufbt FORCE=1` собирает без
      предупреждений компилятора, `dist/tea_timer.fap` создан
- [X] T050 Проверить память и стек на устройстве по `quickstart.md` §6: `free` до запуска и
      после выхода одинаковый, `top` — запас стека Main-потока ≥ ~30 % (≤ ~1.4 КБ из 2 КБ),
      в логах нет `furi_crash`/assert; при нехватке — увеличить `stack_size` в
      `application.fam` с обоснованием в `plan.md`
      — **проверено на устройстве** (прошивка 1.5.1, `tea_timer.fap` из `/ext/apps/Tools/`):
      поток `tea_timer` — стек 2044 Б, минимум свободного 1524 Б (использовано 520 Б,
      запас 74 % ≥ 30 %), `stack_size` менять не нужно; `free` до запуска 101880 Б, после
      четырёх циклов запуск/выход — 101768 Б и дальше не убывает (разница 112 Б —
      фрагментация кучи и метаданные `loader`, между циклами значение стабильно);
      `furi_crash`/assert в логах нет
- [X] T051 Финальный прогон (`unit_tests/test_index.c`): `ufbt launch` → `ufbt cli` →
      `tea_timer_test` (все наборы) с отчётом `Status: PASSED`, `0 failures`, `Leaked: 0`;
      сохранить отчёт в описании изменения и пройти чек-лист готовности
      `specs/001-tea-brew-timer/quickstart.md` §7
      — **выполнено на устройстве**: `20 tests, 101 assertions, 0 failures`,
      `Time: 248 ms`, `Leaked: 0`, `Status: PASSED` (наборы `tea_preset` — 11 тестов,
      `brew_math` — 9; успех подтверждён звуковым уведомлением); чек-лист §7 закрыт:
      сборка/формат/линт — T049, тесты — здесь, S1–S5 — T030/T033/T040/T044, память и
      стек — T050, строки и собственный View — T046

---

## Зависимости и порядок выполнения

### Зависимости фаз

- **Setup (Phase 1)**: без зависимостей, начинается сразу
- **Foundational (Phase 2)**: зависит от Setup, **блокирует все истории**
- **User Stories (Phase 3+)**: зависят от Foundational
  - последовательно по приоритету (P1 → P2 → P3) либо параллельно при наличии
    исполнителей; US2 и US3 независимы друг от друга
- **Polish (Phase 7)**: зависит от всех выбранных историй

### Зависимости историй

- **US1 (P1)**: стартует после Phase 2; требует `lib/tea_preset_load.c` (T022) и
  `brew_view` (T016–T017), а также обработчик выбора файла и правило сессии (T024a,
  T024b, последовательно после T022/T024); зависимостей от других историй нет
- **US2 (P2)**: стартует после Phase 2; функционально опирается на модель и экран из US1
  (T025–T028), но проверяется отдельно сценарием S2
- **US3 (P2)**: стартует после Phase 2; диалог ошибки файла опирается на парсер (T010) и
  `file_browser` (T023); T037 использует `tea_timer_has_presets()` из T034; сцены
  `NoPresets`/`Error` независимы от US1
- **US4 (P3)**: стартует после Phase 2; расширяет сцену заваривания из US1 (T026–T027)

### Внутри историй

- Модели и чистая логика (`lib/`) — до сцен и отрисовки
- Тесты пишутся и **падают** до реализации (T015/T014 → T010/T008 уже выполнены вместе с
  тестами в Phase 2; новые векторы T029 пишутся до правок загрузчика)
- Реализация сцены — до её регистрации в `tea_timer_scene.c` и `tea_timer.c`
- История считается завершённой после прогона `tea_timer_test` со `Status: PASSED` и её
  сценария из `quickstart.md`

### Параллельные возможности

- Все задачи Setup с меткой `[P]` (T002–T004) — параллельно после T001
- Все задачи Foundational с `[P]` (T007, T009, T011, T012, T014, T015) — параллельно
  внутри фазы
- После Foundational истории US2 и US3 могут выполняться разными исполнителями параллельно
- Внутри US1 параллельно: T022, T023, T024 (разные файлы)

---

## Пример параллельного запуска: US1

```bash
# Запустить вместе (разные файлы, нет общих зависимостей):
Task: "T022 [P] [US1] lib/tea_preset_load.h + lib/tea_preset_load.c — слой Storage"
Task: "T023 [P] [US1] scenes/tea_timer_scene_file_select.c — file_browser"
Task: "T024 [P] [US1] scenes/tea_timer_scene_preset_card.c — widget карточки"

# Затем последовательно (общие файлы):
Task: "T024a [US1] scenes/tea_timer_scene_file_select.c — обработчик FileSelected"
Task: "T024b [US1] scenes/tea_timer_scene_file_select.c — правило сессии"
Task: "T025 [US1] views/brew_view.c — раскладки IDLE/RUNNING/DONE"
Task: "T026 [US1] scenes/tea_timer_scene_brew.c — тик, старт/стоп, завершение"
Task: "T027 [US1] scenes/tea_timer_scene_brew.c — NotificationSequence"
Task: "T028 [US1] scenes/tea_timer_scene_config.h + tea_timer_scene.c + tea_timer.c"
```

## Пример параллельного запуска: US3

```bash
Task: "T034 [P] [US3] scenes/tea_timer_scene_no_presets.c"
Task: "T035 [P] [US3] scenes/tea_timer_scene_error.c"
# затем T037, T038 (один файл scenes/tea_timer_scene_file_select.c), T039, T040
```

---

## Стратегия реализации

### Сначала MVP (только US1)

1. Phase 1: Setup — манифест, типы, иконка
2. Phase 2: Foundational — логика `lib/`, юнит-тесты на устройстве, каркас сцен
3. Phase 3: US1 — выбор файла, карточка, отсчёт, сигнал, приращение
4. **СТОП и ПРОВЕРКА**: S1 из `quickstart.md` + `tea_timer_test` → `Status: PASSED`
5. Демонстрация/установка на устройство

### Инкрементальная поставка

1. Setup + Foundational → каркас и тесты готовы
2. US1 → проверка → поставка (MVP)
3. US2 → проверка S2 → поставка (превышение лимита)
4. US3 → проверка S3/S4 → поставка (устойчивость к файлам и карте)
5. US4 → проверка S5 → поставка (точность, подсветка, корректный выход)
6. Polish → `ufbt format`/`ufbt lint`, память, финальный отчёт тестов

### Параллельная работа

1. Setup + Foundational выполняются вместе
2. После Foundational: разработчик A — US1, затем US4; разработчик B — US3; US2 зависит
   от модели US1 и берётся после неё
3. Истории интегрируются через `scenes/tea_timer_scene_config.h` и `tea_timer.c`

---

## Примечания

- `[P]` — разные файлы, нет зависимостей от незавершённых задач
- `[US#]` — трассировка к историям из `spec.md`
- **Сцены 002** (главное меню с пунктом `New preset`, `text_input`, `number_input`,
  `Summary`, `Saved` — C-UI-1, C-UI-9) в этот список **не входят**: это отдельная фича
  `002-preset-management`, у которой ещё нет `plan.md`/`tasks.md`. Меню создаётся с пунктом
  `Presets`, пункт `New preset` добавляется вместе с 002
- `tea_preset_load()` и слой `Storage` появляются только в US1; парсер `tea_preset_parse_buf()`
  от них не зависит и тестируется отдельно
- Вывод тестов — через `snprintf` + `pipe_send` (`printf`, `puts`, `vsnprintf`
  **не экспортируются в FAP** в SDK 1.4.3); команда регистрируется
  `cli_registry_add_command_ex` со стеком 2048 байт
- Тестовая команда включается под `#if defined(FURI_DEBUG) || defined(TEA_TIMER_TESTS)`;
  флаг уточняется в T006
- Все строки интерфейса — короткий английский ASCII; комментарии в коде и вся документация —
  на русском (принцип VII)
- Коммит после каждой задачи или логической группы; остановка на любом checkpoint для
  независимой проверки истории
- Избегать: расплывчатых задач, конфликтов в одном файле внутри `[P]`-группы,
  межсторийных зависимостей, ломающих независимость проверки
