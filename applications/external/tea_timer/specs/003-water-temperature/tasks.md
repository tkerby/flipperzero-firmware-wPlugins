---

description: "Task list for feature implementation"
---

# Tasks: Температура воды — диапазон в пресете, файле и интерфейсе

**Input**: Design documents from `/specs/003-water-temperature/`

**Prerequisites**: plan.md (обязательно), spec.md (обязательно), research.md, data-model.md, contracts/, quickstart.md

**Tests**: Тесты на устройстве **обязательны** — их требуют FR-024 и принцип VIII конституции.
Новых наборов и новой CLI-команды не создаётся: расширяются существующие наборы
`tea_preset`, `preset_write`, `brew_math` (регистрация в `unit_tests/test_index.c` не меняется).

**Organization**: задачи сгруппированы по user story, чтобы каждую историю можно было
реализовать и проверить независимо.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: можно выполнять параллельно (другие файлы, нет зависимости от незавершённых задач)
- **[Story]**: к какой user story относится задача (US1, US2, US3, US4)
- В описании — точный путь к файлу

## Path Conventions

- Однопроектное FAP-приложение: исходники в корне `tea_timer/` (`tea_timer.h`, `tea_timer_i.h`,
  `tea_timer.c`), в `lib/tea_logic/`, `scenes/`, `views/`, `unit_tests/`
- Артефакты спецификации: `specs/003-water-temperature/`
- Все команды сборки и запуска выполняются из каталога приложения, только через `ufbt`
  (`ufbt`, `ufbt launch`, `ufbt cli`, `ufbt format`, `ufbt lint`); `./fbt`/`scons` не используются

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: подготовка окружения, базовые линии сборки/тестов/памяти и недостающие примеры

- [X] T001 [P] Прочитать обязательный скилл по конституции (принцип VI): `.opencode/skills/flipper-zero-app-dev/SKILL.md`, `references/api-gui.md` (модули `number_input` и `widget`), `references/testing.md`, `references/best-practices.md`; сверить сигнатуры `number_input_set_result_callback`, `number_input_set_header_text`, `widget_add_string_element`, `widget_add_button_element` с заголовками SDK 1.5.1 в `~/.ufbt/current/sdk_headers/`
- [X] T002 Зафиксировать базовую линию сборки из каталога приложения: `ufbt`, `ufbt format`, `ufbt lint`; ожидаемо — `dist/tea_timer.fap` собирается без предупреждений, диффа форматирования нет
- [X] T003 Зафиксировать базовую линию тестов на устройстве: `ufbt launch`, затем `ufbt cli` → `tea_timer_test`; сохранить отчёт (`N tests, M assertions, K failures`, `Time:`, `Leaked:`, `Status`) для сравнения после 003
- [X] T004 [P] Зафиксировать базовую линию памяти в `ufbt cli`: `free` и `top` до запуска приложения и после выхода из него (BACK из главного меню) — значения нужны для проверки ограничения «прирост ≤ десятков байт»
- [X] T005 [P] Добавить отсутствующий пример `specs/003-water-temperature/examples/water-90-90.tea` (`water_temp_min_c: 90`, `water_temp_max_c: 90`, остальные ключи как в `examples/water-85-90.tea`) и упомянуть его в `specs/003-water-temperature/examples/README.md` — он нужен для сценария S3 из `quickstart.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: модель данных и шаги формы, общие для всех user story

**⚠️ CRITICAL**: Ни одна user story не начинается, пока эта фаза не завершена

- [X] T006 Добавить в `tea_timer.h` константы `TEA_WATER_TEMP_MIN` = `85`, `TEA_WATER_TEMP_MAX` = `90`, `TEA_WATER_TEMP_DEFAULT_MIN` = `TEA_WATER_TEMP_MIN`, `TEA_WATER_TEMP_DEFAULT_MAX` = `TEA_WATER_TEMP_MAX` и поля `uint8_t water_temp_min_c` (**85–90; ≤ `water_temp_max_c`**) и `uint8_t water_temp_max_c` (**85–90; ≥ `water_temp_min_c`**) в структуру `TeaPreset` между `leaf_tenths_g` и `first_pour_sec`; комментарии — на русском, как в остальном файле (data-model §1)
- [X] T007 [P] Добавить в `lib/tea_logic/tea_preset_save.h` умолчания формы `TEA_DEFAULT_WATER_TEMP_MIN` = `TEA_WATER_TEMP_DEFAULT_MIN` и `TEA_DEFAULT_WATER_TEMP_MAX` = `TEA_WATER_TEMP_DEFAULT_MAX` рядом с `TEA_DEFAULT_LEAF_TENTHS`, обновив комментарий блока умолчаний (сейчас «5.0 г / 20 с / 5 с / 6 проливов»)
- [X] T008 [P] Добавить в `tea_timer_i.h` шаги `TeaEditStepWaterMin` и `TeaEditStepWaterMax` сразу после `TeaEditStepLeaf`, чтобы порядок был `Name → Leaf → WaterMin → WaterMax → First → Increment → MaxPours → Summary` (data-model §3, `contracts/ui-screens.md` §2); убедиться, что уменьшение `edit_step` при BACK в `scenes/tea_timer_scene_edit_number.c` остаётся согласованным с новым порядком
- [X] T009 Проверить сборку после изменения структуры `TeaPreset`: `ufbt` из каталога приложения — весь проект (сцены, `views/`, `unit_tests/`) компилируется без предупреждений; обратить внимание на временные копии структуры в `lib/tea_logic/brew_math.c` и `lib/tea_logic/tea_preset_load.c` (правок не требуется, если значения копируются целиком)

**Checkpoint**: модель и шаги формы готовы — истории можно реализовывать независимо

---

## Phase 3: User Story 1 - Видеть рекомендуемую температуру воды на карточке пресета (Priority: P1) 🎯 MVP

**Goal**: `.tea`-файл с диапазоном читается парсером, а карточка пресета показывает строку
`Water 85-90 C`; файлы без ключей температуры читаются с умолчанием `85–90`, файлы с
неполной парой отклоняются существующим диалогом ошибки.

**Independent Test**: положить в `/ext/apps_data/tea_timer/` файл `water-85-90.tea` и открыть
его через «Presets» — карточка открывается, видна строка `Water 85-90 C` и все прежние
параметры; `legacy-no-water.tea` показывает `Water 85-90 C`; файлы из `examples/invalid/`
не открываются и дают диалог с именем проблемного ключа.

### Tests for User Story 1 (обязательны, принцип VIII) ⚠️

> **NOTE: сначала тесты, они должны падать до реализации T012**

- [X] T010 [US1] Добавить в `unit_tests/lib_tests/tea_preset/tea_preset_test.c` положительные векторы новых ключей: эталонный файл нового формата → `TeaPresetResultOK`, `water_temp_min_c == 85`, `water_temp_max_c == 90`; файл с `water_temp_min_c: 85`/`water_temp_max_c: 87` → `OK`, `85`/`87`; файл с `water_temp_min_c: 87`/`water_temp_max_c: 87` → `OK`, `87`/`87`; файл старого формата без обоих ключей → `OK`, `85`/`90` (FR-006); неизвестный ключ `water_temp: 85-90` → `OK`, поле игнорируется
- [X] T011 [US1] Добавить в `unit_tests/lib_tests/tea_preset/tea_preset_test.c` отрицательные векторы: `water_temp_min_c: 84` → `ErrOutOfRange` с `error_field == "water_temp_min_c"`; `water_temp_max_c: 91` → `ErrOutOfRange` с `"water_temp_max_c"`; `88`/`86` → `ErrOutOfRange` с `"water_temp_max_c"`; только `water_temp_min_c` → `ErrNoField` с `"water_temp_max_c"`; только `water_temp_max_c` → `ErrNoField` с `"water_temp_min_c"`; дубликат каждого ключа → `ErrDuplicateField` с его именем; `85.5` и `85abc` → `ErrBadNumber`; ` 085 ` → `OK`, `85` (FR-002, FR-003, FR-005, FR-007)

### Implementation for User Story 1

- [X] T012 [US1] Расширить парсер в `lib/tea_logic/tea_preset.c` (контракт `contracts/tea-file-format.md` §7): флаги `TEA_FIELD_WATER_MIN (1U << 5)` и `TEA_FIELD_WATER_MAX (1U << 6)`, `TEA_FIELD_ALL` оставить из прежних пяти бит (температура не обязательна); после `memset(out, 0, sizeof(TeaPreset))` установить `out->water_temp_min_c = TEA_WATER_TEMP_DEFAULT_MIN`, `out->water_temp_max_c = TEA_WATER_TEMP_DEFAULT_MAX`; добавить ветки `tea_key_eq(key, key_len, "water_temp_min_c")` / `"water_temp_max_c"` (дубликат — существующая ветка `TeaPresetResultErrDuplicateField` с именем ключа); значение разбирать `tea_parse_uint` (`ErrBadNumber` с именем ключа), затем проверять 85–90 (`ErrOutOfRange` с именем ключа); **после цикла и до проверки `seen != TEA_FIELD_ALL`** отдельно обработать неполную пару: ровно один из двух флагов → `TeaPresetResultErrNoField` с именем **отсутствующего** ключа; оба флага и `min > max` → `TeaPresetResultErrOutOfRange` с `error_field = "water_temp_max_c"`; `tea_first_missing_field()` не менять
- [X] T013 [P] [US1] Добавить строку диапазона на карточку пресета в `scenes/tea_timer_scene_preset_card.c` и пересчитать раскладку 128×64 по `contracts/ui-screens.md` §1.1: название — y=0 (`FontPrimary`), `Leaf %u.%u g/100ml` — y=10, новая строка `Water %u-%u C` — y=20 (`FontSecondary`, значения из `app->session.preset.water_temp_min_c` / `water_temp_max_c`), `Pours %u / %u` — y=30, `Pour %u  %s` — y=40; кнопки `Start` и `>` остаются снизу без изменений (FR-010, FR-011)
- [X] T014 [US1] Прогнать сценарий на устройстве: `ufbt`, `ufbt launch`, `ufbt cli` → `tea_timer_test tea_preset` (`Status: PASSED`, `0 failures`, `Leaked: 0`), затем вручную S1, S2, S3, S4 из `quickstart.md` (карточка показывает `85-90`, `85-87`, `90-90`; невалидные файлы дают `Missing field: …`, `Out of range: …`, `Duplicate field: …`, `Not a number: …` с именем ключа, приложение не падает, кнопки `Retry`/`OK` работают)

**Checkpoint**: US1 полностью работоспособна и проверяема независимо — параметр уже виден на устройстве

---

## Phase 4: User Story 2 - Задавать диапазон температуры при создании пресета на устройстве (Priority: P1)

**Goal**: новые шаги `Water min, C` / `Water max, C` в форме создания, проверка 85–90 и
`min ≤ max`, строка диапазона в сводке, запись обоих ключей в файл и корректное обратное
чтение.

**Independent Test**: создать на устройстве пресет с `85`/`87`, сохранить и открыть через
«Presets» — карточка показывает `Water 85-87 C`, файл на компьютере содержит оба ключа в
порядке контракта.

### Tests for User Story 2 (обязательны, принцип VIII) ⚠️

> **NOTE: сначала тесты, они должны падать до реализации T017**

- [X] T015 [US2] Добавить в `unit_tests/lib_tests/preset_write/preset_write_test.c` тест сериализации и обратного чтения: `tea_preset_serialize()` пишет ровно 7 ключей в порядке `name`, `leaf_g_per_100ml`, `water_temp_min_c`, `water_temp_max_c`, `first_pour_sec`, `increment_sec`, `max_pours`, по одному на строку, с завершающим `\n`; `tea_preset_parse_buf()` на этом буфере даёт те же значения, включая оба значения температуры (FR-004, FR-008, SC-007)
- [X] T016 [US2] Добавить в `unit_tests/lib_tests/preset_write/preset_write_test.c` тест `tea_preset_check_draft_ex()`: `85`/`90` и `87`/`87` → `TeaPresetResultOK`; `84`/`90` → `ErrOutOfRange` с `error_field == "water_temp_min_c"`; `85`/`91` → `ErrOutOfRange` с `"water_temp_max_c"`; `88`/`86` → `ErrOutOfRange` с `"water_temp_max_c"` (FR-002, FR-003, FR-017)

### Implementation for User Story 2

- [X] T017 [US2] Реализовать запись и проверку в `lib/tea_logic/tea_preset_save.c`: `tea_preset_serialize()` — добавить строки `water_temp_min_c: %u\n` и `water_temp_max_c: %u\n` сразу после `leaf_g_per_100ml` (итого 7 ключей, порядок `contracts/tea-file-format.md` §6); `tea_preset_check_draft_ex()` — после проверки граммовки проверять каждое значение на 85–90 (`ErrOutOfRange` с именем ключа) и затем `water_temp_min_c > water_temp_max_c` → `ErrOutOfRange` с `"water_temp_max_c"`; `tea_preset_same_values()` — добавить сравнение обоих полей (иначе рассинхронизация даёт `PresetSaveErrVerify`); обновить doc-комментарии в `lib/tea_logic/tea_preset_save.h` (сейчас «пять ключей», «формат 001»)
- [X] T018 [P] [US2] Заполнить умолчания создания в `scenes/tea_timer_scene_menu.c` при выборе `New preset`: `app->draft.water_temp_min_c = TEA_DEFAULT_WATER_TEMP_MIN;` и `app->draft.water_temp_max_c = TEA_DEFAULT_WATER_TEMP_MAX;` рядом с остальными умолчаниями и обновить комментарий «5.0 / 20 / 5 / 6» (FR-015)
- [X] T019 [P] [US2] Добавить числовые шаги в `scenes/tea_timer_scene_edit_number.c`: для `TeaEditStepWaterMin` — заголовок `Water min, C`, значение `app->draft.water_temp_min_c`, границы 85…90; для `TeaEditStepWaterMax` — заголовок `Water max, C`, значение `app->draft.water_temp_max_c`, границы `app->draft.water_temp_min_c`…90 (равенство допустимо: `87–87`); в callback заменить переход `Leaf → TeaEditStepFirst` на `Leaf → TeaEditStepWaterMin`, добавить `WaterMin → WaterMax` и `WaterMax → First`; BACK уже работает через `edit_step--` — проверить маршруты `WaterMin → Leaf`, `WaterMax → WaterMin`, `First → WaterMax` (FR-014, FR-016, FR-017, `contracts/ui-screens.md` §2)
- [X] T020 [P] [US2] Обновить сводку формы в `scenes/tea_timer_scene_edit_summary.c`: строку граммовки на y=19 заменить на `Leaf %u.%u  Water %u-%u C` (значения из `app->draft`), в `tea_timer_scene_edit_summary_step_for_field()` добавить `"water_temp_min_c" → TeaEditStepWaterMin` и `"water_temp_max_c" → TeaEditStepWaterMax` до финального `return TeaEditStepMaxPours`, в `tea_timer_scene_edit_summary_hint()` добавить подсказку `Water: 85-90 C` для обоих шагов (FR-018, FR-023, `contracts/ui-screens.md` §3)
- [X] T021 [US2] Прогнать на устройстве: `ufbt`, `ufbt launch`, `ufbt cli` → `tea_timer_test preset_write` (`Status: PASSED`, `Leaked: 0`), затем вручную S5 и S9 из `quickstart.md` (шаги `Water min, C` = `85` и `Water max, C` = `90`; сводка `Leaf 5.0  Water 85-90 C`; границы 85…90 недоступны к выходу; `min` ограничивает снизу шаг максимума); сохранить файл и убедиться на компьютере, что оба ключа записаны в порядке контракта

**Checkpoint**: US1 и US2 работают независимо; созданные на устройстве пресеты читаются как созданные вручную

---

## Phase 5: User Story 3 - Правка диапазона у существующего пресета (Priority: P2)

**Goal**: правка существующего пресета открывает шаги температуры с текущими значениями,
меняет только отредактированные поля, сохраняет имя файла и дописывает пару ключей в файл
старого формата.

**Independent Test**: открыть `legacy-no-water.tea`, `>` → `Edit`, изменить только максимум
на `88`, сохранить — файл с прежним именем содержит `85`/`88` и оба ключа, карточка
показывает `Water 85-88 C`; при отмене файл не изменён.

### Tests for User Story 3 (обязательны, принцип VIII) ⚠️

- [X] T022 [P] [US3] Добавить в `unit_tests/lib_tests/preset_write/preset_write_test.c` тест правки файла старого формата: черновик из парсера старого файла (`85`/`90`) с изменённым `water_temp_max_c = 88` записать через `tea_preset_save(..., editing = true, ...)` в `APP_DATA_PATH("test")` — путь прежний, файл содержит оба ключа, остальные значения не изменились, обратное чтение совпало (FR-019, FR-020, FR-022)

### Implementation for User Story 3

- [X] T023 [P] [US3] Дополнить `tea_timer_scene_edit_summary_sync_session()` в `scenes/tea_timer_scene_edit_summary.c`: копировать `app->draft.water_temp_min_c` и `app->draft.water_temp_max_c` в `app->session.preset` рядом с остальными полями, чтобы карточка сразу после сохранения показывала новый диапазон без перезапуска (FR-019, FR-026); проверить, что `scenes/tea_timer_scene_actions.c` копирует `draft = session.preset` целиком и шаги температуры открываются с текущими значениями
- [X] T024 [US3] Прогнать на устройстве сценарий S6 из `quickstart.md`: `Edit` у `legacy-no-water.tea` открывает `Water min, C` = `85` и `Water max, C` = `90`; изменение только максимума на `88` даёт карточку `Water 85-88 C`, прежнее имя файла и сохранные прочие значения; отмена (BACK) на любом шаге не меняет файл и оставляет прежний диапазон на карточке (FR-019…FR-021); также прогнать `tea_timer_test preset_write`

**Checkpoint**: все три P1/P2-истории независимо функциональны

---

## Phase 6: User Story 4 - Экран заваривания остаётся сосредоточен на времени (Priority: P3)

**Goal**: отрицательное требование — на экране заваривания (`IDLE`, `RUNNING`, `DONE`)
нет ни одной надписи о температуре, раскладка совпадает с 001, а значение при этом
доступно на карточке после BACK.

**Independent Test**: открыть `water-85-90.tea`, нажать `Start`, дождаться `Time is up!` —
во всех трёх состояниях на экране нет `Water`, `C` и чисел 85–90; состав и раскладка как в
001; BACK возвращает карточку с `Water 85-90 C`.

### Tests for User Story 4 (обязательны, принцип VIII) ⚠️

- [X] T025 [US4] Добавить в `unit_tests/lib_tests/brew_math/brew_math_test.c` тест сохранности значения без показа: пресет с `water_temp_min_c = 85`, `water_temp_max_c = 87` после `brew_session_reset_from_preset()` сохраняет оба поля в `session.preset` (FR-026 — значение не теряется, хотя на экран не выводится); в комментарии теста указать ссылку на FR-012/FR-027

### Verification for User Story 4

- [X] T026 [US4] Выполнить аудит отсутствия температуры на экране заваривания: `grep -n "Water\|water_temp\|temp\| C" views/brew_view.c views/brew_view.h scenes/tea_timer_scene_brew.c lib/tea_logic/brew_math.c lib/tea_logic/brew_math.h` — совпадений о температуре быть не должно; **правок в `views/brew_view.c`, `views/brew_view.h` и `lib/tea_logic/brew_math.*` не вносить** (FR-012, FR-027, SC-009)
- [X] T027 [US4] Прогнать на устройстве сценарий S7 из `quickstart.md`: открыть `water-85-90.tea`, `Start` (ожидание запуска), отсчёт пролива, `Time is up!` — проверить полный текст экрана во всех трёх фазах (нет `Water`, `C`, чисел 85–90), раскладка и элементы совпадают с 001; BACK возвращает карточку с `Water 85-90 C`; дополнительно `tea_timer_test brew_math` → `Status: PASSED`

**Checkpoint**: все четыре user story функциональны независимо

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: стиль, документация, финальные прогоны и приёмка

- [X] T028 [P] Привести код к стилю: `ufbt format` из каталога приложения — дифф допускается только в изменённых файлах (`tea_timer.h`, `tea_timer_i.h`, `lib/tea_logic/tea_preset.c`, `lib/tea_logic/tea_preset_save.c`, `lib/tea_logic/tea_preset_save.h`, `scenes/tea_timer_scene_edit_number.c`, `scenes/tea_timer_scene_edit_summary.c`, `scenes/tea_timer_scene_menu.c`, `scenes/tea_timer_scene_preset_card.c`, тесты)
- [X] T029 [P] Обновить `README.md` (английский, принцип VII): добавить строки `water_temp_min_c` и `water_temp_max_c` в таблицу ключей (`85`–`90`, integer), обновить пример `.tea` (7 ключей), описать умолчание `85`/`90` для файлов без пары ключей, ошибку для неполной пары и формат отображения `Water 85-90 C`
- [X] T030 [P] Добавить в `specs/001-tea-brew-timer/contracts/tea-file-format.md` короткую пометку о расширении 003 со ссылкой на `specs/003-water-temperature/contracts/tea-file-format.md` (историю 001 не переписывать, принцип VII/G11)
- [X] T031 [P] Обновить комментарий в `tea_timer.c` около `number_input_alloc()` — «Один экземпляр number_input на все четыре числовых шага формы» изменить на шесть шагов, чтобы комментарий соответствовал `tea_timer_i.h`; логику не менять
- [X] T032 Финальная сборка и статический анализ: `ufbt` (без предупреждений) и `ufbt lint` (без неразобранных замечаний) из каталога приложения
- [X] T033 Полный прогон тестов и проверка памяти на устройстве: `ufbt launch`, `ufbt cli` → `tea_timer_test` (`Status: PASSED`, `0 failures`, `Leaked: 0`), затем `free` и `top` до запуска приложения и после выхода — сравнить с базовой линией T004 (прирост в пределах обычного шума, не более десятков байт; новых потоков нет)
- [X] T034 Финальная приёмка по `quickstart.md`: пройти S1–S9 (повторно, после всех правок), сверить с матрицей приёмки §6 и закрыть чек-лист §7; приложить отчёт `tea_timer_test` к описанию изменения

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: без зависимостей — можно начинать сразу
- **Foundational (Phase 2)**: зависит от Setup — блокирует все user story
- **User Stories (Phase 3–6)**: зависят от Foundational
  - после Foundational истории можно вести параллельно (при наличии исполнителей)
  - либо последовательно по приоритету: US1 → US2 → US3 → US4
- **Polish (Phase 7)**: зависит от всех выбранных историй

### User Story Dependencies

- **US1 (P1)**: после Foundational; не зависит от других историй (парсер + карточка)
- **US2 (P1)**: после Foundational; использует модель из Foundational, но функционально независима
  от US1 (форма и запись); на практике выполняется после US1, т.к. `T017` меняет `tea_preset_save.c`,
  а `T012` — `tea_preset.c` (разные файлы, конфликтов нет)
- **US3 (P2)**: после Foundational; требует шагов формы из US2 (`T019`) и проверки из `T017`;
  отдельная ценность — правка файла старого формата и синхронизация сессии
- **US4 (P3)**: после Foundational; **не вносит изменений в экран заваривания** — только
  регрессионная проверка и аудит; зависит от US1 (диапазон должен читаться и отображаться)

### Within Each User Story

- Тесты (`T010`, `T011`, `T015`, `T016`, `T022`, `T025`) пишутся и **падают** до реализации
- Модель/парсер (`T012`, `T017`) — до сцен, которые на них опираются
- Реализация сцены — до прогона сценария на устройстве
- История завершена только после `Status: PASSED` на устройстве и прохождения её ручных сценариев

### Parallel Opportunities

- Setup: `T001`, `T004`, `T005` независимы; `T002` → `T003` (запуск требует собранного FAP)
- Foundational: `T007` и `T008` — параллельно с `T006` (разные файлы); `T009` — после всех
- US1: `T013` (карточка) параллельно с `T010`/`T011` (тесты парсера); `T012` — после тестов
- US2: тесты `T015`/`T016` — в одном файле, строго последовательно; `T018`, `T019`, `T020` —
  три разных файла, все параллельны; `T017` — после тестов
- US3: `T022` (тесты) и `T023` (сцена сводки) — параллельно
- US4: `T025` (тест `brew_math`) и `T026` (аудит) — параллельно
- Polish: `T028`–`T031` — параллельно; `T032` → `T033` → `T034` последовательно

---

## Parallel Example: User Story 1

```bash
# Параллельно: тесты парсера и раскладка карточки (разные файлы)
Task: "T010/T011 — векторы парсера в unit_tests/lib_tests/tea_preset/tea_preset_test.c"
Task: "T013 — строка Water %u-%u C и раскладка в scenes/tea_timer_scene_preset_card.c"

# После падающих тестов — реализация парсера
Task: "T012 — разбор water_temp_min_c/water_temp_max_c в lib/tea_logic/tea_preset.c"
```

## Parallel Example: User Story 2

```bash
# Параллельно: умолчания формы, шаги number_input, сводка (три разных файла)
Task: "T018 — умолчания 85/90 в scenes/tea_timer_scene_menu.c"
Task: "T019 — шаги Water min/Water max в scenes/tea_timer_scene_edit_number.c"
Task: "T020 — строка Leaf … Water … C и маппинг ошибок в scenes/tea_timer_scene_edit_summary.c"
```

## Parallel Example: User Story 3

```bash
# Параллельно: тест правки старого файла и синхронизация сессии (разные файлы)
Task: "T022 — тест правки в unit_tests/lib_tests/preset_write/preset_write_test.c"
Task: "T023 — копирование температуры в сессию в scenes/tea_timer_scene_edit_summary.c"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Завершить Phase 1: Setup
2. Завершить Phase 2: Foundational (критично — блокирует всё остальное)
3. Завершить Phase 3: US1 (парсер + карточка)
4. **ОСТАНОВИТЬСЯ и ПРОВЕРИТЬ**: S1–S4 по `quickstart.md`, `tea_timer_test tea_preset` → PASSED
5. Демонстрация: диапазон уже виден на устройстве для файлов, подготовленных на компьютере

### Incremental Delivery

1. Setup + Foundational → модель готова
2. + US1 → проверка → деплой (MVP: рекомендация видна на карточке)
3. + US2 → проверка → деплой (пресеты с диапазоном создаются на устройстве; закрыт весь P1)
4. + US3 → проверка → деплой (правка диапазона и миграция файлов старого формата)
5. + US4 → проверка → деплой (подтверждено отсутствие температуры на таймере, раскладка 001 сохранена)
6. Polish → финальная приёмка S1–S9, `ufbt lint`, `Leaked: 0`, отчёт тестов

### Suggested MVP Scope

**US1** — минимальный полезный инкремент (6 задач: `T010`–`T014` с учётом Phase 1–2).
Полный приоритет P1 = **US1 + US2**: без ввода на устройстве теряется двусторонняя
совместимость формата, поэтому P1-инкрементом считается пара US1+US2.

### Совместимость и ограничения (контроль по ходу работ)

- `application.fam`, `tea_timer.c` (логика), `views/brew_view.*`, `lib/tea_logic/brew_math.*`,
  `scenes/tea_timer_scene_file_select.c`, `unit_tests/test_index.c` — без изменений
- новых `View`/`ViewId`, сцен, модулей GUI, потоков и аллокаций не появляется
- стек Main-потока 2 КБ не увеличивается; буферы `char buffer[48]`/`char text[64]` достаточны
  (самая длинная строка — `Missing field: water_temp_max_c`, 31 символ)
- все строки интерфейса — английский ASCII: `Water min, C`, `Water max, C`, `Water %u-%u C`
- формат `.tea` расширяется только аддитивно: файлы 001/002 остаются читаемыми (SC-003)

---

## Состояние выполнения (2026-10-29)

Выполнено: все задачи `T001`–`T034`.

Результаты прогонов (устройство: Flipper Zero, прошивка 1.5.1, `ufbt`):

- `ufbt` — сборка без предупреждений; `ufbt lint` — замечаний нет; `ufbt format` — дифф только
  в изменённых файлах.
- Базовая линия до 003: `tea_timer_test` → `33 tests, 198 assertions, 0 failures`,
  `Leaked: 0`, `Status: PASSED`.
- После 003: `tea_timer_test` → `40 tests, 273 assertions, 0 failures`, `Leaked: 0`,
  `Status: PASSED`; по наборам: `tea_preset` — `14 tests, 93 assertions`,
  `preset_write` + `brew_math` — без падений.
- Память: `free` до запуска приложения — 108664 Б, при работающем приложении — 88776 Б,
  после `loader close` — 118832 Б (прирост в пределах обычного шума десктопа,
  утечек нет, новых потоков нет: `top` показывает один поток `tea_timer`, стек 2044 → 1516 Б).
- Аудит `T026`: в `views/brew_view.c/.h`, `scenes/tea_timer_scene_brew.c`,
  `lib/tea_logic/brew_math.c/.h` нет строк о температуре; файлы не изменялись.

Ручная проверка на устройстве (`T014`, `T021`, `T024`, `T027`, `T034`) — сценарии S1–S9
из `quickstart.md` пройдены пользователем вручную; отклонений не зафиксировано.
Автоматический ввод по USB при этом не использовался (см. ограничения в
`.opencode/skills/flipper-zero-app-dev/references/rpc-ui-testing.md`): модальные диалоги
приложения блокируют закрытие процесса, поэтому сценарии S1–S4 подтверждены снимками
экрана, а S5–S9 — ручным прогоном.

## Notes

- [P] — разные файлы, нет зависимости от незавершённых задач
- [Story] связывает задачу с user story из `spec.md` для трассируемости
- Каждая история самостоятельно завершаема и проверяема на устройстве
- Тесты не проходят → слияние блокируется (принцип VIII конституции)
- Коммит после каждой задачи или логической группы
- Останавливаться на чекпоинтах для независимой проверки истории
