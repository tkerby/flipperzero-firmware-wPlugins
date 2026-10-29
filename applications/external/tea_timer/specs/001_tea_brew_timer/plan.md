# Implementation Plan: Tea Timer — таймер заваривания чая проливами

**Branch**: `001-tea-brew-timer` | **Date**: 2026-10-28 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-tea-brew-timer/spec.md`

**Основание пересмотра**: конституция проекта переведена на **версию 3.0.1**. Ключевые
изменения, повлиявшие на план:

- принцип **VIII** — юнит-тесты ОБЯЗАНЫ исполняться **на устройстве**: тестовый код
  компилируется в FAP, запускается CLI-командой через `ufbt cli`, отчёт печатается в
  консоль по образцу `polarikus/flipper-zero_fn_tester` (minunit + `test_index.c`);
  схема «тесты на хосте через `cc`/`make`» из предыдущей редакции плана **отменена**;
- принцип **VII** — вся документация на русском, английский только `README.md` в корне;
  английские строки интерфейса закреплены как техническое ограничение платформы;
- принцип **VI** — обязательное использование скилла
  `.opencode/skills/flipper-zero-app-dev` и сверка с заголовками SDK;
- принципы **I–V** — FAP-only, C + Furi API, переиспользование экранов прошивки,
  `ufbt` как единственный toolchain, ограниченная память и неблокирующий GUI-поток.

## Summary

Внешнее приложение (FAP) `tea_timer` для Flipper Zero: таймер заваривания чая проливами.
Пресеты — текстовые файлы `.tea` в каталоге данных приложения на SD-карте. Пользователь
выбирает пресет, видит карточку (название, граммовка, счётчик проливов, длительность
следующего пролива), запускает пролив по OK; по истечении времени получает однократный
звук + вибро, счётчик растёт на 1, длительность следующего пролива увеличивается на
`increment_sec`. Превышение рекомендованного числа проливов не блокируется и показывается
как `9 / 8` + плашка `OVER`.

Технический подход:

- каркас — `ViewDispatcher` + `SceneManager` (`ViewDispatcherTypeFullscreen`);
- экраны — **готовые модули GUI прошивки** (`submenu`, `file_browser`, `widget`, `popup`,
  `text_input`, `number_input`, `dialogs`) + **один** собственный `View` на Canvas только
  для экрана заваривания;
- отсчёт — `view_dispatcher_set_tick_event_callback` (1 Гц) против дедлайна
  `furi_get_tick()`, без отдельного потока;
- сигнал/вибро — `notification` + собственный `NotificationSequence`; экран не гаснет —
  `sequence_display_backlight_enforce_on` / `..._auto`;
- логика разделена на чистую (`lib/tea_preset.c`, `lib/brew_math.c`, без Furi) и
  привязанную к I/O (`lib/tea_preset_load.c`), что делает её пригодной для юнит-тестов;
- **тестирование — на устройстве**: `unit_tests/` с minunit, реестром наборов
  `test_index.c`, CLI-командой `tea_timer_test` и отчётом в консоль (принцип VIII).

## Technical Context

**Language/Version**: C11 + расширения GNU; сборка `ufbt` (SDK прошивки 1.4.3, toolchain
`arm-none-eabi-gcc` из `~/.ufbt`)

**Primary Dependencies**: только API прошивки — `furi`, `gui` (`view_dispatcher`,
`scene_manager`, `gui/modules/*`, `gui/elements.h`), `dialogs`, `storage`, `notification`,
`cli` (для тестовой команды). Сторонних библиотек нет; `minunit` вендорится в
`unit_tests/` как один header.

**Storage**: SD-карта, `APP_DATA_PATH("")` = `/ext/apps_data/tea_timer/`; пресеты — файлы
`.tea` (UTF-8/ASCII, ≤ 1 КБ). Доступ только через `Storage`. Тестовые файлы создаются во
временном подкаталоге тестов и удаляются после прогона.

**Testing**: **обязательный прогон на устройстве** — `unit_tests/` компилируется в FAP,
команда `tea_timer_test [suite]` в `ufbt cli`, отчёт `N tests, M assertions, K failures`,
`Time: <ms> ms` (замер `furi_get_tick()` до и после прогона — обязательное поле по
принципу VIII), `Leaked:`, `Status: PASSED|FAILED`; результат дублируется уведомлением. Дополнительно —
`ufbt lint` (статический анализ), `ufbt format` (стиль), ручные сценарии из
`quickstart.md`. Прогон на хосте допускается только как вспомогательная отладка и
зачётным не является (принцип VIII).

**Target Platform**: Flipper Zero, STM32WB55 (Cortex-M4F, 256 КБ RAM), экран 128×64,
1 бит/пиксель; прошивка с поддержкой FAP.

**Project Type**: single project — одно FAP-приложение.

**Performance Goals**: реакция на кнопку ≤ 100 мс; точность отсчёта ±1 с на интервале до
60 с (SC-003) и корректное отображение без переполнения на всём диапазоне 1–3600 с;
открытие каталога пресетов ≤ 3 с при ≤ 100 файлах; отрисовка только по событиям;
полный прогон юнит-тестов ≤ 3 с на устройстве.

**Constraints**: RAM FAP ограничена; стек Main-потока 2 КБ; буферы фиксированного размера
в `TeaTimerApp` и в куче; в GUI-потоке нет `furi_delay_ms` и блокирующего I/O; тестовый
код активен только в отладочной конфигурации и не влияет на обычный режим; все ресурсы
парные.

**Scale/Scope**: 1 приложение, сцены `Menu`/`FileSelect`/`PresetCard`/`Brew`/`NoPresets`/
`Error`, 1 собственный View, ~8 экранов UI, до 100 пресетов, 5 обязательных полей `.tea`,
2 набора юнит-тестов (`tea_preset`, `brew_math`) с ~45 утверждениями.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Проверка по действующей конституции (`/Users/igordanilov/CLionProjects/flipper/tea_timer/.specify/memory/constitution.md`,
версия 3.0.1, принципы I–VIII).

| # | Гейт | Принцип | Статус | Обоснование |
|---|---|---|---|---|
| G1 | Только внешнее приложение, без правок прошивки | I | PASS | Код только в каталоге приложения и `specs/`; `application.fam` с `apptype=EXTERNAL`. |
| G2 | Только C и Furi API | II | PASS | Все взаимодействия через Furi; `lib/` использует только libc с границами; логи через `FURI_LOG_*`. |
| G3 | Сначала готовые экраны прошивки | III | PASS | 7 из 8 экранов 001 — стандартные модули; один собственный `brew_view` обоснован (R-UI-2). |
| G4 | `ufbt` — единственный toolchain | IV | PASS | Сборка/запуск/стиль/анализ: `ufbt`, `ufbt launch`, `ufbt format`, `ufbt lint`. |
| G5 | Ограниченная память, неблокирующий GUI | V | PASS | Tick-callback вместо потока; буферы в структуре/куче; парность ресурсов описана ниже. |
| G6 | Скилл проекта обязателен | VI | PASS | План и research построены по `flipper-zero-app-dev` + заголовкам SDK 1.4.3 из `~/.ufbt`. |
| G7 | Язык артефактов: русский, кроме `README` | VII | PASS | Все артефакты на русском; английский — только строки UI (техническое ограничение платформы) и `README.md` в корне. |
| G8 | Юнит-тесты на устройстве | VIII | **PASS (после пересмотра)** | `unit_tests/` + minunit + CLI-команда `tea_timer_test` + отчёт `Status: PASSED`; host-прогон исключён из зачётных. |
| G9 | Тестовый код под отладочным флагом | VIII | PASS | Регистрация команды под `FURI_DEBUG` (резерв — `cdefines=["TEA_TIMER_TESTS"]`), в релизе поведение не меняется. |
| G10 | Английские ASCII-строки UI, 128×64 | разд. «Технические ограничения» | PASS | Строки зафиксированы в `contracts/ui-screens.md`, все короткие, латиница. |
| G11 | Формат `.tea` — публичный контракт | разд. «Технические ограничения» | PASS | Формат и алгоритм разбора зафиксированы в `contracts/tea-file-format.md`, покрыты тестами. |

**Нарушений нет, Complexity Tracking не требуется** — кроме обоснования единственного
собственного View (R-UI-2) и присутствия тестового кода в FAP (ниже).

**Пост-дизайн проверка (после Phase 1)**: гейты G1–G11 перепроверены по `research.md`,
`data-model.md`, `contracts/*`, `quickstart.md`. Все PASS: тестовая часть плана теперь
описывает только прогон на устройстве, host-тесты переведены в разряд вспомогательных,
дизайн-артефакты ссылаются на `unit_tests/` и CLI-команду.

## Переиспользование стандартных экранов (уточнение пользователя)

Проверено по заголовкам SDK 1.4.3 в
`~/.ufbt/current/sdk_headers/f7_sdk/applications/services/gui/modules/` (набор:
`button_menu`, `button_panel`, `byte_input`, `dialog_ex`, `empty_screen`, `file_browser`,
`loading`, `menu`, `number_input`, `popup`, `submenu`, `text_box`, `text_input`,
`variable_item_list`, `widget`) и `dialogs/dialogs.h`.

| # | Экран | Готовый компонент | Ключевые вызовы | Своего кода |
|---|---|---|---|---|
| 01 | Главное меню `Presets` / `New preset` (002 FR-001) | **`submenu`** | `submenu_alloc`, `submenu_set_header("Tea Timer")`, `submenu_add_item` | нет |
| 02 | Выбор пресета — системный выбор файла (001 FR-006 заменён) | **`file_browser`** | `file_browser_alloc(result_path)`, `file_browser_configure(b, ".tea", APP_DATA_PATH(""), false, true, &I_tea_file, true)` (`tea_timer_icons.h`), `file_browser_start`, `file_browser_set_callback` | нет |
| 03 | Карточка пресета = экран заваривания в ожидании `IDLE` (001 FR-009/010) | **`widget`** | `widget_add_string_element`, `widget_add_button_element(Center, "Start", …)` — `Start` сразу запускает отсчёт | нет |
| 04 | Отсчёт (FR-018) | **`brew_view`** (единственный свой) | `canvas_set_font(FontBigNumbers)`, `canvas_draw_str_aligned` | 1 View |
| 05 | `Time is up!` (FR-013/019) | состояние `DONE` в `brew_view` (альтернатива `popup` отклонена: нужен крупный отсчёт и плашка) | `canvas_draw_str_aligned` | часть draw |
| 06 | Превышение `9 / 8 OVER` (FR-014) | флаг `over_limit` в `brew_view` (`canvas_draw_rbox`), не отдельная фаза | — | часть draw |
| 07 | ~~Файл пропущен~~ (FR-007 в 001 не требует счётчика) | не реализуется в 001: причина показывается диалогом C-UI-10 | — | — |
| 08 | Пресетов нет (FR-008) | **`widget`** + кнопка `Reload` | `widget_add_string_element`, `widget_add_button_element` | нет |
| 09 | SD-карта недоступна | **`dialogs`** | `dialog_message_show_storage_error()` | нет |
| 10 | Ошибка проверки файла (001 FR-007) | **`dialogs`** `DialogMessage` | `dialog_message_set_header/text/buttons`, `dialog_message_show` | нет |
| 11 | Ввод названия (002 FR-003) | **`text_input`** | `text_input_set_header_text`, `text_input_set_result_callback` | нет |
| 12 | Ввод чисел (002 FR-004/FR-006) | **`number_input`** | `number_input_set_result_callback(input, cb, ctx, &value, min, max)` | нет |
| 13 | Сводка/подтверждение (002 FR-007) | **`widget`** + `DialogMessage` `Yes/No` | — | нет |
| 14 | «Сохранено: file.tea» (002 FR-009) | **`popup`** | — | нет |
| 15 | Долгое чтение/старт | **`loading`** | `loading_alloc`, `loading_get_view` | нет |

Не берём: `menu` (нужны иконки-ассеты), `variable_item_list` (редактор значений в
списке), блокирующий `dialog_file_browser_show` (ломает модель сцен), `text_box`,
`button_panel`, `button_menu`, `byte_input`.

## Тестирование на устройстве (принцип VIII)

Схема повторяет `polarikus/flipper-zero_fn_tester` (изучены `unit_tests/minunit.h`,
`minunit_vars.h`, `test_index.c`, наборы `lib_tests/fn/*_test.c`).

### Структура

```text
unit_tests/
├── minunit.h              # вендорный minunit (MU_TEST, MU_RUN_SUITE, MU_REPORT, mu_assert_*)
├── minunit_vars.h         # счётчики run/assert/fail/status, таймеры, last_message
├── test_index.h           # tea_timer_tests_register / unregister
├── test_index.c           # реестр наборов + CLI-команда + печать отчёта
└── lib_tests/
    ├── tea_preset/tea_preset_test.c   # run_minunit_test_tea_preset()
    └── brew_math/brew_math_test.c     # run_minunit_test_brew_math()
```

### Раннер и команда

- Реестр: `typedef struct { const char* name; int (*entry)(void); } UnitTest;`, массив
  `unit_tests[]` с именами `tea_preset`, `brew_math`.
- Регистрация: `cli_registry_add_command_ex(registry, "tea_timer_test", CliCommandFlagParallelSafe, cb, NULL, 2048)`
  на `RECORD_CLI`; снятие — `cli_registry_delete_command` при выходе (принцип VIII
  конституции 3.0.1; имя символа сверено с `api_symbols.csv` SDK 1.4.3).
- Аргумент команды — необязательное имя набора (`tea_timer_test brew_math`); без
  аргумента прогоняются все.
- Доступность: под `#ifdef FURI_DEBUG` (фактический флаг ufbt-сборки уточняется на
  первом шаге реализации через `ufbt VERBOSE=1`); если флаг не определяется —
  `cdefines=["TEA_TIMER_TESTS"]` в `application.fam` и явное включение при прогоне.
- GUI не блокируется: команда исполняется в потоке CLI; `furi_delay_ms` в тестах
  допустим (это не GUI-поток).
- Отчёт в консоль:
  `snprintf(line, sizeof(line), "%s()\r\n", #test)` + `pipe_send(pipe, …)`
  (`printf`/`puts`/`vsnprintf` в FAP не экспортируются — см. tasks.md T011/T013) → при
  падении `minunit_print_fail(...)` → по завершении
  `MU_REPORT()` (тесты/утверждения/падения) → `Leaked: <delta>` по
  `memmgr_get_free_heap()` до и после → `Status: PASSED|FAILED`, плюс
  `sequence_success` / `sequence_error` на устройстве.
- `MINUNIT_MESSAGE_LEN` уменьшается до 256 (экономия RAM, сообщений хватает).

### Что покрывается

| Набор | Покрытие |
|---|---|
| `tea_preset` | 5 обязательных полей и дубликаты; все коды `TeaPresetResult`, включая `ERR_BAD_NAME`; границы 0.1/50.0, 1/3600, 0/600, 1/99; граммовка без точки (`5`) и с лишней цифрой (`5.00`); BOM/CRLF/`#`/пробелы; мусор после числа; `name` пустой, > 24 и с недопустимыми символами; размер 1024/1025 (через `tea_preset_parse_buf`); `tea_preset_load()` на временном файле в `APP_DATA_PATH("test")` с удалением после прогона |
| `brew_math` | `next = prev + increment`, отсутствие роста при `increment_sec = 0`, предел 63000 с, форматирование `mm:ss` и `hh:mm:ss`, `over_limit` на границе, инкремент счётчика только по завершении, отсутствие инкремента при прерывании, `brew_session_start()` (дедлайн и повторный старт), `brew_session_remaining_sec()` (округление вверх, ноль за дедлайном), `brew_session_reset_from_preset()` |

Парсер разделён на два слоя: `tea_preset_parse_buf()` (чистая, вход — буфер) и
`tea_preset_load()` (Storage + I/O). Тесты вызывают оба слоя на устройстве; слоистость
нужна не для host-прогона, а чтобы каждая ошибка проверялась отдельным утверждением.

## Project Structure

### Documentation (this feature)

```text
specs/001-tea-brew-timer/
├── plan.md              # этот файл
├── research.md          # Phase 0: решения и обоснования
├── data-model.md        # Phase 1: сущности, поля, состояния
├── quickstart.md        # Phase 1: сборка, прогон тестов на устройстве, приёмка
├── contracts/
│   ├── ui-screens.md    # контракт экранов
│   ├── tea-file-format.md  # контракт формата .tea
│   └── storage-layout.md   # контракт каталога и имён файлов
├── checklists/requirements.md
├── ui/                  # референсные макеты
└── tasks.md             # Phase 2 (/speckit.tasks, создаётся не здесь)
```

### Source Code (repository root)

```text
tea_timer/
├── application.fam             # appid tea_timer, entry_point tea_timer_app,
│                               # requires=[gui, storage, notification, dialogs, cli],
│                               # stack_size=2048, cdefines=["TEA_TIMER_TESTS"] (при необходимости)
├── tea_timer.c                 # entry point, alloc/free, ViewDispatcher + SceneManager,
│                               # регистрация/снятие тестовой команды под FURI_DEBUG
├── tea_timer.h                 # публичные типы (TeaPreset, TeaTimerApp)
├── tea_timer_i.h               # enum ViewId, состояние приложения
├── tea_timer_custom_event.h    # enum TeaTimerCustomEvent
├── scenes/
│   ├── tea_timer_scene.h
│   ├── tea_timer_scene_config.h
│   ├── tea_timer_scene.c             # массив tea_timer_scene_handlers[]
│   ├── tea_timer_scene_menu.c        # submenu
│   ├── tea_timer_scene_file_select.c # file_browser
│   ├── tea_timer_scene_preset_card.c # widget = IDLE + Start
│   ├── tea_timer_scene_brew.c        # brew_view + tick + сигнал
│   ├── tea_timer_scene_no_presets.c  # widget + Reload
│   └── tea_timer_scene_error.c       # dialogs/popup
├── views/
│   ├── brew_view.c             # единственный собственный View (RUNNING/DONE + флаг OVER)
│   └── brew_view.h
├── lib/
│   ├── tea_preset.c            # parse_buf: разбор и валидация (без Furi)
│   ├── tea_preset.h
│   ├── tea_preset_load.c       # Storage-обёртка над parse_buf
│   ├── tea_preset_load.h
│   ├── brew_math.c             # длительность пролива, форматирование, переходы сессии
│   └── brew_math.h
├── unit_tests/                 # юнит-тесты, исполняются НА УСТРОЙСТВЕ (принцип VIII)
│   ├── minunit.h
│   ├── minunit_vars.h
│   ├── test_index.h
│   ├── test_index.c
│   └── lib_tests/
│       ├── tea_preset/tea_preset_test.c
│       └── brew_math/brew_math_test.c
├── images/
│   ├── 10px.png                # fap_icon
│   └── tea_file_10px.png       # fap_icon_assets → I_tea_file
└── README.md                   # единственный англоязычный документ (принцип VII)
```

`dist/tea_timer.fap` — результат `ufbt`.

**Structure Decision**: один самодостаточный FAP-проект в корне рабочего каталога.
Слои: `lib/` (логика без GUI), `views/` (своя отрисовка), `scenes/` (навигация и
побочные эффекты), `unit_tests/` (тесты в составе FAP), `tea_timer.c` (жизненный цикл и
владение ресурсами). Архитектура сразу включает сцены 002 (главное меню, `file_browser`,
`text_input`/`number_input`).

### Порядок владения ресурсами

`tea_timer_alloc()`:
1. `furi_record_open(RECORD_GUI, RECORD_NOTIFICATION, RECORD_STORAGE, RECORD_DIALOGS)`.
2. `view_dispatcher_alloc()` + `scene_manager_alloc(&tea_timer_scene_handlers, app)`.
3. callbacks: custom/navigation/tick (1 Гц).
4. модули: `submenu`, `file_browser`, `widget` (карточка, пустой каталог)
   и собственный `brew_view`; `popup`, `text_input`, `number_input`, `loading`
   добавляются вместе с 002, в 001 не аллоцируются.
5. `view_dispatcher_attach_to_gui(..., ViewDispatcherTypeFullscreen)`.
6. `scene_manager_next_scene(..., TeaTimerSceneMenu)`.

Под `FURI_DEBUG` дополнительно: `tea_timer_tests_register()` (открывает `RECORD_CLI`
внутри функции и закрывает его там же, как в fn_tester).

`tea_timer_free()` — строго обратный порядок: `tea_timer_tests_unregister()` →
`remove_view` для каждого view → `view_dispatcher_free` → `scene_manager_free` →
`*_free` модулей → `furi_string_free(result_path)` → `furi_record_close` × 4 → `free(app)`.

## Порядок реализации (крупные шаги)

1. Каркас: `application.fam`, `tea_timer.c`, сцены-заглушки, `submenu` — приложение
   запускается и выходит по BACK.
2. `lib/tea_preset.c` + `lib/brew_math.c` + `unit_tests/` (раннер, `test_index.c`,
   два набора) — прогон `tea_timer_test` на устройстве даёт `Status: PASSED`.
3. `file_browser` + `tea_preset_load` + диалог ошибки файла.
4. `brew_view` + `tick` + `notification` + подсветка + переходы состояний.
5. Сцены «нет пресетов» / «SD-ошибка» / «пропущен файл».
6. Сцены 002: создание пресета (`text_input`, `number_input`, сводка, запись, `popup`).
7. Прогон полного чек-листа `quickstart.md`, `ufbt format`, `ufbt lint`, отчёт тестов.

## Отклонения при реализации (фазы 1–3)

Зафиксированы по факту сборки и прогона на устройстве; не меняют поведение, но
уточняют пути и размеры из tasks.md.

| Решение по плану/tasks | Факт | Причина |
|---|---|---|
| `lib/brew_math.c`, `lib/tea_preset.c`, `lib/tea_preset_load.c` | `lib/tea_logic/*` + `fap_private_libs=[Lib(name="tea_logic", sources=["*.c"])]` | ufbt всегда исключает каталог `lib/` из исходников приложения (`GatherSources([..., "!lib"])`); приватная библиотека — штатный способ держать код в `lib/` |
| `PresetError.error_field[16]` | `char error_field[24]` | ключ `leaf_g_per_100ml` — 16 символов + `\0`, иначе диалог `Missing field:` обрезал имя |
| SDK 1.4.3 (API 87.1) | SDK 1.5.1 (API 88.2) | устройство обновлено пользователем до прошивки 1.5.1; SDK выровнен `ufbt update -c release` |
| `FURI_DEBUG` как флаг тестов | `cdefines=["TEA_TIMER_TESTS"]` в манифесте | ufbt-сборка определяет `FURI_NDEBUG`/`NDEBUG`, `FURI_DEBUG` отсутствует (T006) |
| `&I_tea_file` из `fap_icon_assets` | `&I_tea_file_10px` при файле `images/tea_file_10px.png` | генератор ассетов сохраняет суффикс размера в имени символа |
| `view_set_context(app)` внутри `brew_view_alloc()` | устанавливается в `tea_timer.c` после `brew_view_alloc()` | прототип `brew_view_alloc(void)` из T016 не принимает контекст |
| `BrewViewModel` без служебных полей | добавлено `FuriString* name_string` | `elements_string_fit_width` требует `FuriString`, а аллокации в `draw_callback` запрещены (V) |

## Complexity Tracking
| Сложность | Почему нужна | Отвергнутая альтернатива |
|---|---|---|
| Собственный `brew_view` (Canvas) | нужны `FontBigNumbers` для крупного отсчёта и инверсия плашек `OVER`/счётчика; `widget` этого не умеет (R-UI-2) | `widget` с пересборкой элементов каждую секунду — нет инверсии, мигание, больше кода |
| Тестовый код (`unit_tests/`, ~4 файла) в составе FAP | прямое требование принципа VIII: тесты исполняются на устройстве | host-тесты через `cc` — запрещены конституцией 3.0.1 как зачётный способ |
| Тестовые наборы активны под `FURI_DEBUG` | не увеличивать RAM/поведение релизной сборки | всегда включённая CLI-команда — лишний код и поверхность в релизе |
| Разделение `tea_preset.c` / `tea_preset_load.c` | позволяет проверять разбор без SD и отдельно проверять I/O | один модуль с `Storage` внутри — часть ошибок невозможно изолировать в тесте |
