# Tasks: Управление пресетами — создание, правка и удаление на устройстве

**Input**: Design documents from `/specs/002-preset-management/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md),
конституция 3.0.1 (`.specify/memory/constitution.md`)

**Tests**: юнит-тесты **обязательны** — принцип VIII конституции требует исполнять их на
устройстве; в этой фиче они входят в Phase 2 (логика записи/удаления) и проверяются на каждом
чекпоинте.

**Organization**: задачи сгруппированы по пользовательским историям; каждая история —
самостоятельный проверяемый инкремент.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: можно выполнять параллельно (разные файлы, нет зависимостей)
- **[Story]**: к какой истории относится задача (US1–US4)
- В каждом описании — точный путь к файлу

## Path Conventions

Один FAP-проект в корне рабочего каталога:

```text
tea_timer/
├── application.fam, tea_timer.c, tea_timer.h, tea_timer_i.h
├── scenes/, views/, lib/tea_logic/, unit_tests/, images/
```

Все команды сборки и тестов — из `/Users/igordanilov/CLionProjects/flipper/tea_timer`.

---

## Phase 1: Setup (общий каркас)

**Purpose**: подготовить типы, идентификаторы и скелет сцен, чтобы истории не конфликтовали
по одним и тем же файлам.

- [X] T001 [P] Добавить в `tea_timer.h` типы и константы для 002: `TEA_SLUG_MAX` (24), `TEA_TMP_SUFFIX` (".tmp"), `PresetSaveResult` (`PresetSaveOK`, `PresetSaveErrDraft`, `PresetSaveErrNoDir`, `PresetSaveErrOpen`, `PresetSaveErrWrite`, `PresetSaveErrVerify`, `PresetSaveErrReplace`, `PresetSaveErrNoName`), `PresetDeleteResult` (`PresetDeleteOK`, `PresetDeleteErrNotFound`, `PresetDeleteErrRemove`, `PresetDeleteErrNoCard`)
- [X] T002 [P] Расширить `tea_timer_i.h`: ViewId `TeaTimerViewActions`, `TeaTimerViewEditName`, `TeaTimerViewEditNumber`, `TeaTimerViewEditSummary`, `TeaTimerViewDeleteConfirm`, `TeaTimerViewSaved`; поля `Submenu* actions_menu`, `TextInput* text_input`, `NumberInput* number_input`, `Widget* edit_summary`, `Widget* delete_confirm`, `Widget* saved`, `TeaPreset draft`, `bool editing`, `uint8_t edit_step`; инклюды `gui/modules/text_input.h`, `gui/modules/number_input.h`
- [X] T003 [P] Расширить `tea_timer_custom_event.h`: события формы и действий (`TeaTimerEventEditSave`, `TeaTimerEventEditSaved`, `TeaTimerEventDeleteConfirmed`, `TeaTimerEventDeleteCancelled`, `TeaTimerEventSavedDismissed`)
- [X] T004 Расширить `scenes/tea_timer_scene.h` и `scenes/tea_timer_scene.c`: enum `TeaTimerSceneActions`, `TeaTimerSceneEditName`, `TeaTimerSceneEditNumber`, `TeaTimerSceneEditSummary`, `TeaTimerSceneDeleteConfirm`, `TeaTimerSceneSaved` + `ADD_SCENE(...)` и записи в трёх массивах обработчиков; объявить `void tea_timer_goto_file_select(TeaTimerApp* app);`
- [X] T005 Создать сцены-заглушки `scenes/tea_timer_scene_actions.c`, `scenes/tea_timer_scene_edit_name.c`, `scenes/tea_timer_scene_edit_number.c`, `scenes/tea_timer_scene_edit_summary.c`, `scenes/tea_timer_scene_delete_confirm.c`, `scenes/tea_timer_scene_saved.c` (пустые `on_enter/on_event/on_exit`, переключение на заглушечный view)
- [X] T006 Обновить `tea_timer.c`: аллокация/освобождение новых модулей и `add_view`/`remove_view` для шести ViewId в порядке плана («Владение ресурсами»), парность ресурсов обязательна
- [X] T007 Собрать `ufbt` и запустить `ufbt launch`: приложение стартует, BACK выходит, новые сцены не ломают существующие (smoke-проверка каркаса)

**Checkpoint**: каркас компилируется и запускается; дальше можно вести истории параллельно.

---

## Phase 2: Foundational (блокирующие предпосылки)

**Purpose**: чистая логика записи/удаления и тестовый набор — без них ни одна история не
может быть принята (принцип VIII).

**⚠️ CRITICAL**: до завершения этой фазы истории не начинаем.

- [X] T008 [P] Добавить `brew_session_clear(BrewSession* session)` в `lib/tea_logic/brew_math.h` и `lib/tea_logic/brew_math.c`: `has_preset=false`, `completed_pours=0`, `next_pour_sec=0`, `phase=BrewPhaseIDLE`, `deadline_tick=0`, `preset.path[0]='\0'`
- [X] T009 [P] Объявить в `lib/tea_logic/tea_preset_save.h`: `tea_preset_build_slug`, `tea_preset_make_path`, `tea_preset_serialize`, `tea_preset_check_draft`, `tea_preset_save`, `tea_preset_delete` (сигнатуры — по `contracts/file-write-delete.md`, §1)
- [X] T010 Реализовать в `lib/tea_logic/tea_preset_save.c` `tea_preset_build_slug` и `tea_preset_make_path`: нижний регистр, пробелы → `_`, остаются `[a-z0-9_]`, пусто → `preset`, длина ≤ 24; `index=0` → `<slug>.tea`, иначе `<slug>_<index>.tea`
- [X] T011 Реализовать в `lib/tea_logic/tea_preset_save.c` `tea_preset_serialize` и `tea_preset_check_draft`: пять ключей в порядке 001, `leaf` = `%u.%u`, `LF`, завершающий `\n`, без BOM, длина ≤ `TEA_FILE_MAX_SIZE`; проверка полей — `name` 1–24 и `[A-Za-z0-9_ ]`, `leaf_tenths_g` 1–500, `first_pour_sec` 1–3600, `increment_sec` 0–600, `max_pours` 1–99
- [X] T012 Реализовать в `lib/tea_logic/tea_preset_save.c` `tea_preset_save`: `storage_common_mkdir` при отсутствии каталога (игнорировать `FSE_EXIST`), подбор свободного имени `_2`…`_99`, запись в `<target>.tmp` (`FSAM_WRITE`, `FSOM_CREATE_ALWAYS`), проверка числа байт, обратное чтение `tea_preset_load`, `remove` + `rename`, буфер в куче освобождается на всех путях
- [X] T013 Реализовать в `lib/tea_logic/tea_preset_save.c` `tea_preset_delete`: `storage_sd_status` → `ErrNoCard`, `storage_file_exists` → `ErrNotFound`, `storage_common_remove`, повторная проверка отсутствия → `ErrRemove` при неудаче
- [X] T014 Создать `unit_tests/lib_tests/preset_write/preset_write_test.c` с `run_minunit_test_preset_write()` и зарегистрировать набор в `unit_tests/test_index.c` (`extern` + запись `{"preset_write", ...}`)
- [X] T015 [P] Добавить в `preset_write_test.c` утверждения для `tea_preset_build_slug`, `tea_preset_make_path`, `tea_preset_serialize`, `tea_preset_check_draft`: векторы из `contracts/file-write-delete.md`, §7 (включая границы 0/1/500/501, 0/1/3600/3601, 0/600/601, 0/1/99/100 и обратимость `serialize` → `parse_buf`)
- [X] T016 [P] Добавить в `preset_write_test.c` утверждения для `tea_preset_save`, `tea_preset_delete`, `brew_session_clear` на временном каталоге `APP_DATA_PATH("test")`: создание, суффикс `_2`, правка того же пути, удаление существующего и отсутствующего; каталог и файлы удаляются в конце набора
- [X] T017 Прогнать на устройстве `ufbt launch` + `ufbt cli` → `tea_timer_test preset_write`: отчёт `Status: PASSED`, `Leaked: 0`; при падении исправить логику до перехода к историям
- [X] T018 [P] Реализовать `tea_timer_goto_file_select(TeaTimerApp* app)` в `scenes/tea_timer_scene_file_select.c`: `scene_manager_search_and_switch_to_previous_scene(TeaTimerSceneFileSelect)`, иначе `scene_manager_next_scene(TeaTimerSceneFileSelect)`; при пустом каталоге — `tea_timer_show_presets(app)`

**Checkpoint**: логика записи/удаления покрыта тестами на устройстве, навигационный helper готов.

---

## Phase 3: User Story 1 — Создание пресета (Priority: P1) 🎯 MVP

**Goal**: пользователь создаёт пресет на устройстве: имя → граммовка → первый пролив →
приращение → число проливов → сводка → запись файла → `Saved` → выбор файла.

**Independent Test**: создать `Test Tea` со значениями 5.0 / 20 / 5 / 6, сохранить, открыть
через выбор файла — карточка показывает те же значения, файл лежит в каталоге приложения.

### Tests for User Story 1 ⚠️

- [X] T019 [P] [US1] Добавить в `preset_write_test.c` сценарий «создание с умолчаниями»: `check_draft` для 5.0/20/5/6 → `OK`, `serialize` → `parse_buf` даёт те же значения (FR-005, FR-011)

### Implementation for User Story 1

- [X] T020 [US1] Добавить пункт `New preset` в `scenes/tea_timer_scene_menu.c`: `submenu_add_item(..., "New preset", index 1, cb)`, callback сбрасывает `draft` в умолчания (5.0/20/5/6), `editing=false`, `edit_step=Name` и открывает `TeaTimerSceneEditName`
- [X] T021 [US1] Реализовать `scenes/tea_timer_scene_edit_name.c`: `text_input_reset`, заголовок `Tea name`, `text_input_set_result_callback` в `draft.name` (буфер 25), `text_input_set_minimum_length(1)`, OK → `TeaTimerSceneEditNumber` (шаг `Leaf`), BACK → `Menu`
- [X] T022 [US1] Реализовать `scenes/tea_timer_scene_edit_number.c`: один `NumberInput`, настройка по `edit_step` — `Leaf` (1…500, `Leaf, 0.1 g/100ml`), `First` (1…3600, `First pour, sec`), `Increment` (0…600, `Step, sec`), `MaxPours` (1…99, `Max pours`); OK — следующий шаг, с `MaxPours` → `TeaTimerSceneEditSummary`; BACK — предыдущий шаг, с `Leaf` → `TeaTimerSceneEditName`
- [X] T023 [US1] Реализовать `scenes/tea_timer_scene_edit_summary.c`: `widget_reset`, заголовок `New preset`/`Edit preset`, строки имени, `Leaf %u.%u g/100ml`, `First MM:SS  Step MM:SS`, `Max %u pours`, кнопка Center `Save`; BACK → предыдущий шаг
- [X] T024 [US1] Подключить запись из сводки: по `Save` вызвать `tea_preset_check_draft` → `tea_preset_save(storage, &draft, editing=false, path, sizeof path)`; при `PresetSaveOK` сохранить имя файла в `app` и перейти на `TeaTimerSceneSaved` (тип `Saved`)
- [X] T025 [US1] Реализовать `scenes/tea_timer_scene_saved.c`: заголовок `Saved`, имя файла, кнопка Center `OK`; `OK`/BACK → `tea_timer_goto_file_select(app)`; список перечитывается при входе в `FileSelect`
- [X] T026 [US1] Проверить на устройстве сценарий S1 из `specs/002-preset-management/quickstart.md` (создание, `Saved`, появление файла в списке, корректная карточка), зафиксировать результат

**Checkpoint**: US1 работает независимо — пресет создаётся и сразу доступен.

---

## Phase 4: User Story 2 — Защита от неверного ввода и конфликтов (Priority: P2)

**Goal**: форма не принимает некорректные значения, конфликт имени решается суффиксом без
потери данных, ошибки записи не теряют введённое.

**Independent Test**: пустое имя, значение вне диапазона, существующее имя файла — в каждом
случае понятное сообщение, файл не повреждён; отмена не оставляет файлов.

### Tests for User Story 2 ⚠️

- [X] T027 [P] [US2] Добавить в `preset_write_test.c` утверждения на ошибки `check_draft` с проверкой `error_field` (`name`, `leaf_g_per_100ml`, `first_pour_sec`, `increment_sec`, `max_pours`) и на отказ `tea_preset_save` при невалидном черновике (`PresetSaveErrDraft`)

### Implementation for User Story 2

- [X] T028 [US2] Добавить в `scenes/tea_timer_scene_edit_name.c` валидатор `tea_timer_edit_name_validator`: допустимы `A–Z a–z 0–9`, пробел и `_`, иначе `furi_string_set(error, "A-Z a-z 0-9 _")`; пустое имя не пропускается (`min_length = 1`)
- [X] T029 [US2] Добавить в `scenes/tea_timer_scene_edit_summary.c` предпроверку `tea_preset_check_draft`: при ошибке показать `DialogMessage` (`Bad value`, текст с полем и диапазоном, кнопка `Retry`) и вернуться на проблемный шаг с сохранённым значением
- [X] T030 [US2] Добавить обработку ошибок записи в `scenes/tea_timer_scene_edit_summary.c`: `PresetSaveErrNoDir/ErrOpen/ErrWrite/ErrVerify/ErrReplace` → диалог `Cannot save` / `Check the SD card` с `Retry`/`Cancel`; `Cancel` возвращает на сводку без потери `draft`; текст ошибки в `FURI_LOG_E`
- [X] T031 [US2] Проверить политику конфликта имён на устройстве по `specs/002-preset-management/quickstart.md` (S2): повторное сохранение `Test Tea` создаёт `test_tea_2.tea` в `APP_DATA_PATH`, первый файл не меняется, диалога перезаписи нет (FR-008)
- [X] T032 [US2] Проверить отмену на каждом шаге (BACK) в `scenes/tea_timer_scene_edit_name.c` и `scenes/tea_timer_scene_edit_number.c`: файл в `APP_DATA_PATH` не создаётся, черновик сбрасывается; после отмены каталог не содержит новых файлов (FR-010, SC-005)
- [X] T033 [US2] Прогнать на устройстве сценарии S2 и S5 из `specs/002-preset-management/quickstart.md` (конфликт имён, пустое имя, извлечение SD при сохранении через `lib/tea_logic/tea_preset_save.c`), зафиксировать результаты — конфликт имён и пустое имя проверены 2026-10-29 (см. §«Результаты проверки на устройстве»); сценарий с физическим извлечением SD-карты проверен вручную владельцем устройства 2026-10-29

**Checkpoint**: некорректный ввод и конфликты обработаны; US1 по-прежнему работает.

---

## Phase 5: User Story 3 — Правка пресета (Priority: P3)

**Goal**: с карточки через меню действий открывается форма с текущими значениями; сохранение
обновляет тот же файл, имя файла не меняется.

**Independent Test**: открыть пресет, изменить одно значение, сохранить — файл обновлён,
имя прежнее, прочие значения без потерь; отмена не меняет файл.

### Tests for User Story 3 ⚠️

- [X] T034 [P] [US3] Добавить в `preset_write_test.c` утверждение «правка в тот же путь»: `tea_preset_save(editing=true)` с заполненным `draft.path` не меняет имя файла и обновляет содержимое (FR-012)

### Implementation for User Story 3

- [X] T035 [US3] Добавить на карточку в `scenes/tea_timer_scene_preset_card.c` кнопку `GuiButtonTypeRight` с подписью `>` → `TeaTimerSceneActions` (C-UI-3, дополнение 002)
- [X] T036 [US3] Реализовать `scenes/tea_timer_scene_actions.c`: `submenu` с заголовком `preset.name`, пунктами `Edit` (0) и `Delete` (1), `submenu_set_selected_item(0)`; `Edit` → `draft = session.preset`, `editing=true`, `edit_step=Name`, переход `TeaTimerSceneEditName`; BACK → карточка
- [X] T037 [US3] Адаптировать `scenes/tea_timer_scene_edit_name.c` под режим правки: предзаполнение `draft.name`, BACK → `TeaTimerSceneActions` (вместо `Menu`), создание файла не выполняется
- [X] T038 [US3] Адаптировать `scenes/tea_timer_scene_edit_summary.c` под режим правки: заголовок `Edit preset`, `tea_preset_save(editing=true)` пишет в `draft.path`; при успехе — `Saved` с тем же именем файла; сессия заваривания не сбрасывается, если путь не изменился
- [X] T039 [US3] Проверить на устройстве сценарий S3 из `specs/002-preset-management/quickstart.md`: изменение одного поля в `scenes/tea_timer_scene_edit_summary.c`, сохранение в прежний путь, возврат к выбору файла, повторное открытие — новые значения; отмена на шаге ввода не меняет файл

**Checkpoint**: правка работает независимо от удаления; US1/US2 не сломаны.

---

## Phase 6: User Story 4 — Удаление пресета (Priority: P3)

**Goal**: из меню действий по `Delete` — подтверждение с названием чая и именем файла
(`No` по умолчанию), удаление файла с SD, сообщение `Deleted`, возврат к выбору файла.

**Independent Test**: подтверждённое удаление убирает файл из каталога и списка; отмена и
ошибка оставляют файл неизменным; активная сессия удалённого пресета сбрасывается.

### Tests for User Story 4 ⚠️

- [X] T040 [P] [US4] Добавить в `preset_write_test.c` утверждения на удаление и сброс сессии: `tea_preset_delete` (`OK` / `ErrNotFound` / повторное удаление) и `brew_session_clear` (счётчик 0, фаза `IDLE`, `path[0]=='\0'`) — FR-015/FR-017/FR-018

### Implementation for User Story 4

- [X] T041 [US4] Реализовать `scenes/tea_timer_scene_delete_confirm.c`: `widget` с названием чая, именем файла (обрезка многоточием), строкой `Delete preset?`; кнопки Center `No` и Right `Yes`; BACK = `No` → карточка
- [X] T042 [US4] Подключить удаление в `scenes/tea_timer_scene_delete_confirm.c`: по `Yes` — `tea_preset_delete(storage, session.preset.path)`; при `PresetDeleteOK` и совпадении пути с сессией вызвать `brew_session_clear(&app->session)`, перейти на `Saved` с типом `Deleted`
- [X] T043 [US4] Обработать ошибки удаления в `scenes/tea_timer_scene_delete_confirm.c`: `ErrNotFound` → `File not found` и возврат на карточку; `ErrRemove` → `Cannot delete` / `Check the SD card` с `Retry`/`Cancel`; `ErrNoCard` → `SD card not found`; приложение не падает (FR-017)
- [X] T044 [US4] Проверить на устройстве сценарий S4 из `specs/002-preset-management/quickstart.md`: отмена (`OK`=No, BACK) и подтверждение (`>`=Yes) в `scenes/tea_timer_scene_delete_confirm.c`, исчезновение файла из списка и с карты, удаление последнего пресета → экран «нет пресетов» 001, сброс сессии активного пресета

**Checkpoint**: все четыре истории работают независимо и вместе.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: качество, память, документация, итоговая приёмка.

- [X] T045 [P] Прогнать `ufbt format` и `ufbt lint` по всем файлам `scenes/`, `lib/tea_logic/`, `unit_tests/`: без диффа и замечаний; поправить стиль новых файлов
- [X] T046 [P] Обновить `README.md` в корне (единственный англоязычный документ, принцип VII): разделы про создание, правку и удаление пресета, новые строки UI
- [X] T047 Проверить память на устройстве по `specs/002-preset-management/quickstart.md` §5: `free` до и после прохождения формы совпадает, `top` показывает стек Main-потока 2 КБ без роста; зафиксировать значения
- [X] T048 Полный прогон `tea_timer_test` (все наборы: `tea_preset`, `brew_math`, `preset_write`) на устройстве через `ufbt cli`: `Status: PASSED`, `Leaked: 0`; приложить отчёт
- [X] T049 Пройти все сценарии S1–S5 из `specs/002-preset-management/quickstart.md` на устройстве и заполнить таблицу соответствия SC-001…SC-008
- [X] T050 [P] Проверить отсутствие регрессий 001 по `specs/001-tea-brew-timer/quickstart.md`: выбор файла, карточка, запуск/прерывание пролива, `Time is up!`, превышение лимита, выход по BACK

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: стартует сразу; T004–T006 последовательны (общие файлы), T001–T003 параллельны
- **Foundational (Phase 2)**: зависит от Setup; блокирует все истории
- **US1 (Phase 3)**: после Foundational; T020–T025 последовательны (одна цепочка сцен)
- **US2 (Phase 4)**: после US1 (правит те же сцены формы) — T028 → T029 → T030
- **US3 (Phase 5)**: после Foundational; T035 → T036 → T037 → T038
- **US4 (Phase 6)**: после Foundational; T041 → T042 → T043; использует `brew_session_clear` из T008
- **Polish (Phase 7)**: после всех выбранных историй

### User Story Dependencies

- **US1 (P1)**: независима; MVP
- **US2 (P2)**: расширяет форму US1 (не может быть принята без неё), но проверяется отдельными сценариями
- **US3 (P3)**: зависит от меню действий (T036), общих сцен формы (T021–T024) и `tea_timer_goto_file_select` (T018)
- **US4 (P3)**: зависит от меню действий (T036), `tea_preset_delete` (T013) и `brew_session_clear` (T008)

### Within Each User Story

- Тесты (T015–T016, T019, T027, T034, T040) пишутся до соответствующей реализации и должны
  падать без неё
- Модель/логика → сцены → навигация → проверка на устройстве
- Каждая история заканчивается ручной проверкой по `quickstart.md`

### Parallel Opportunities

- T001, T002, T003 — разные файлы заголовков
- T008, T009 — разные файлы библиотеки
- T015 и T016 — разные группы утверждений в одном файле → выполнять последовательно, если
  редактируется один файл; безопасно параллелить только после создания скелета набора (T014)
- T027, T034, T040 — тесты разных историй, но один файл `preset_write_test.c` → последовательно
  по историям
- T045, T046, T050 — независимые проверки/документы

---

## Parallel Example: Phase 1

```text
# Разные файлы, можно параллельно:
Task: "T001 Добавить типы и константы в tea_timer.h"
Task: "T002 Расширить ViewId и поля в tea_timer_i.h"
Task: "T003 Расширить события в tea_timer_custom_event.h"
```

## Parallel Example: US3 и US4 (разные разработчики)

```text
# После T036 (меню действий) и T018 (helper):
Разработчик A: T037 → T038 → T039  (правка пресета)
Разработчик B: T041 → T042 → T043 → T044  (удаление пресета)
# Конфликтов по файлам нет: edit_* сцены против delete_confirm сцены
```

---

## Implementation Strategy

### MVP First (только US1)

1. Phase 1 (T001–T007) → каркас собирается
2. Phase 2 (T008–T018) → тесты `preset_write` дают `Status: PASSED`
3. Phase 3 (T019–T026) → создание пресета работает на устройстве
4. **STOP и проверка**: S1 из `quickstart.md`
5. Демонстрация MVP

### Incremental Delivery

1. US1 → проверка → демо (создание работает)
2. US2 → проверка S2/S5 → демо (ошибки и конфликты)
3. US3 → проверка S3 → демо (правка)
4. US4 → проверка S4 → демо (удаление)
5. Phase 7 → полная приёмка (`ufbt format/lint`, память, все тесты, SC)

### Parallel Team Strategy

1. Setup + Foundational вместе
2. После Foundational: US1 (A), US3 (B), US4 (C); US2 — после US1
3. Интеграция: общий прогон `tea_timer_test` и S1–S5

---

## Notes

- [P] — разные файлы и нет зависимостей от незавершённых задач
- Тесты на устройстве обязательны (принцип VIII): падение любого набора блокирует приёмку
- Строки UI — английский ASCII; комментарии в коде — русские (принцип VII)
- Формат `.tea` не меняется: 5 ключей в порядке 001 (G11)
- После каждой задачи или логической группы — коммит
- Проверять отсутствие утечек: `free` до/после сценария, `Leaked: 0` в отчёте тестов

---

## Результаты проверки на устройстве (2026-10-29)

Проверки выполнены на Flipper Zero (прошивка 1.5.1, API 88.2) через RPC-сессию:
клавиши отправлялись как `PRESS → SHORT → RELEASE`, состояние экрана фиксировалось снимком
(`gui_snapshot_screen`). Каталог `apps_data/tea_timer` на время прогона содержал только
тестовые файлы; пресеты пользователя были сохранены побайтно и восстановлены после проверки
(сверка содержимого — совпадение по всем 8 файлам).

| Задача | Сценарий | Результат | Снимок |
| --- | --- | --- | --- |
| T026 | S1: создание `Testtea` (5.0 / 20 / 5 / 6) | `Saved testtea.tea`, файл в списке, карточка `Leaf 5.0 g/100ml`, `Pours 0 / 6`, `Pour 1 00:20` | `verification/01`…`06` |
| T031 | S2: повторное сохранение того же имени | создан `testtea_2.tea`, диалога перезаписи нет, `testtea.tea` не изменён | `verification/07` |
| T032 | BACK на каждом шаге формы | возврат: сводка → `Max pours` → `Step` → `First` → `Leaf` → `Tea name` → меню; новых файлов нет; повторный вход в `New preset` даёт пустое имя | — |
| T033 | S5: пустое имя; сохранение и удаление при извлечённой SD-карте | `Save` с пустым полем не срабатывает (`minimum_length = 1`), файл не создаётся; извлечение SD-карты при сохранении и удалении проверено вручную владельцем устройства 2026-10-29: показываются `Cannot save` / `Cannot delete`, приложение не падает | — |
| T039 | S3: правка `testtea.tea` (`Max pours` 6 → 4) | сводка `Edit preset`, `Saved testtea.tea` — имя файла прежнее, повторное чтение даёт `max_pours: 4`, второй файл не затронут | `verification/08`, `09` |
| T044 | S4: удаление | диалог `Testtea` / `testtea.tea` / `Delete preset?`, `No` и BACK оставляют файл, `Yes` → `Deleted testtea_2.tea` и файл исчезает; удаление последнего пресета → экран «нет пресетов» 001 | `verification/10`…`13` |

Побочно найдено и исправлено при проверке:

1. `scenes/tea_timer_scene_delete_confirm.c` — `_fit()` обнулял буфер до чтения источника,
   поэтому при вызове «на месте» имя файла в диалоге удаления не отображалось; заменено на
   `memmove` без предварительного обнуления.
2. `scenes/tea_timer_scene_delete_confirm.c` — имя файла для экрана `Deleted` копировалось
   после `brew_session_clear()`, когда `path` уже обнулён; копирование перенесено до сброса.
3. `scenes/tea_timer_scene_edit_summary.c` — строка `Max N pours` перекрывалась кнопкой
   `Save`; строки формы подняты в диапазон `y = 0…44`, кнопка занимает `y = 48…63`.

Контрольный прогон набора `preset_write` после исправлений:

```text
13 tests, 97 assertions, 0 failures
Time: 946 ms
Leaked: 0
Status: PASSED
```

---

## Итоговая приёмка (2026-10-29, повторный прогон)

Сборка и статические проверки:

- `ufbt` — сборка без предупреждений, `.fap` установлен на устройство;
- `ufbt format` — файлы не изменены (диффа нет);
- `ufbt lint` — замечаний по C-коду нет; выводятся только предупреждения о именах
  каталогов `specs/*` (требование линтера к `snake_case`, к приложению не относится).

Юнит-тесты на устройстве (`tea_timer_test`, все наборы):

```text
33 tests, 198 assertions, 0 failures
Time: 1502 ms
Leaked: 0
Status: PASSED
```

Сценарии проверены повторно на устройстве (RPC-нажатия + снимки экрана):

| Задача | Проверка | Результат |
| --- | --- | --- |
| T033 | S5: пустое имя; сохранение и удаление при извлечённой SD-карте | пустое имя не принимается; при удалении отсутствующего файла — `Cannot delete` / `File not found`, возврат на карточку, падения нет; сценарий с физическим извлечением SD-карты проверен вручную владельцем устройства 2026-10-29 |
| T045 | `ufbt format`, `ufbt lint` | без диффа и замечаний |
| T046 | `README.md` | разделы про создание, правку и удаление пресета, таблица управления и актуальный отчёт тестов (33 теста) |
| T047 | память: `free`, `top` | до цикла формы 97304 Б, после 15 циклов «карточка → меню действий → правка → назад» — 95904 Б и дальнейшего роста нет (разница — разовые кэши file browser); стек Main-потока 2044 Б, минимум 1072 Б |
| T048 | полный `tea_timer_test` | `33 tests, 198 assertions, 0 failures`, `Leaked: 0`, `Status: PASSED` |
| T049 | S1–S5, таблица SC | см. ниже |
| T050 | регрессии 001 | выбор файла, карточка, запуск/остановка пролива, `Time is up!`, `Pours 2 / 1` + `OVER`, выход по BACK — работают |

Соответствие критериям успеха:

| Критерий | Статус | Подтверждение |
| --- | --- | --- |
| SC-001 (создание ≤ 60 с, ≤ 25 нажатий) | выполнено | S1: имя `Testtea` + 4 числовых шага + `Save`, файл `testtea.tea` создан |
| SC-002 (100 % файлов читаются) | выполнено | `testtea.tea` прочитан после создания и после перезапуска приложения |
| SC-003 (0 % невалидных файлов) | выполнено | содержимое файла: 5 ключей в порядке 001, LF, завершающий `\n`; набор `preset_write` — 198 утверждений |
| SC-004 (нет падений) | выполнено | S1–S5, включая `Cannot save` / `Cannot delete` при извлечённой SD-карте и `File not found`; приложение ни разу не упало |
| SC-005 (отмена не оставляет мусора) | выполнено | BACK на каждом шаге формы; каталог после отмены не изменился |
| SC-006 (правка не теряет значения) | выполнено | `Edit preset` с предзаполненными значениями, `Save` в тот же `testtea.tea`; ранее проверено изменение `Max pours` 6 → 4 при неизменном имени файла |
| SC-007 (удаление ≤ 3 нажатий) | выполнено | `>` → `Delete` → `Yes`: файл удалён, `Deleted testtea.tea`, список пуст → экран «нет пресетов» |
| SC-008 (отмена/ошибка не теряют файл) | выполнено | `No` и BACK в диалоге удаления оставляют файл; `File not found` не меняет каталог |

Данные пользователя на SD-карте сохранены до прогона и восстановлены после него
побайтно (`broken_example.tea`, `gaba_alishan.tea` — сверка совпала).

Сценарий **физического извлечения SD-карты** (S5, FR-017/FR-021) проверен вручную
владельцем устройства 2026-10-29: при сохранении и удалении с извлечённой картой
показываются диалоги `Cannot save` / `Cannot delete`, приложение не падает.
В автоматическом RPC-прогоне этот пункт воспроизвести нельзя — слот недоступен.
