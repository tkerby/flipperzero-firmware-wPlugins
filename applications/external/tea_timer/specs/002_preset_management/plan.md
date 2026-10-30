# Implementation Plan: Управление пресетами — создание, правка и удаление на устройстве

**Branch**: `002-preset-management` | **Date**: 2026-10-29 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-preset-management/spec.md`

**Связанные артефакты**: `001-tea-brew-timer` (выбор файла, карточка, таймер, формат
`.tea`, каркас приложения) — переиспользуется без изменений; `003-water-temperature`
(диапазон температуры воды) — будущее расширение формата, в этом плане не реализуется.

## Summary

Фича добавляет к уже работающему приложению `tea_timer` три сценария, выполняемых целиком
на Flipper Zero без компьютера:

- **создание** пресета: название → граммовка → первый пролив → приращение → число проливов
  → сводка → запись `.tea` в `APP_DATA_PATH`;
- **правка** существующего пресета через меню действий на карточке: та же форма с текущими
  значениями, запись в тот же файл (имя файла не меняется), сводка и подтверждение;
- **удаление** пресета: меню действий → подтверждение (`No` по умолчанию) → удаление файла
  с SD-карты → сообщение `Deleted` → возврат к выбору файла.

Технический подход:

- каркас 001 (`ViewDispatcher` + `SceneManager`, `ViewDispatcherTypeFullscreen`)
  расширяется сценами, а не переписывается;
- **все новые экраны — готовые модули GUI**: `submenu` (меню действий), `text_input`
  (название), `number_input` (четыре числа), `widget` (сводка, подтверждение удаления,
  `Saved`), `dialogs` (`DialogMessage` для ошибок) — своих `View` не добавляется;
- логика записи/удаления разделена на **чистую** (`tea_preset_serialize`,
  `tea_preset_build_slug`, `tea_preset_check_draft`) и **привязанную к Storage**
  (`tea_preset_save`, `tea_preset_delete`), поэтому проверяется юнит-тестами на устройстве;
- запись выполняется во временный файл с обратным чтением и только затем подменяет целевой —
  так ошибка записи не уничтожает существующий пресет;
- после сохранения/удаления навигация сводится к экрану выбора файла без «мёртвых» сцен в
  стеке (`tea_timer_goto_file_select`).

## Technical Context

**Language/Version**: C11 + расширения GNU; сборка `ufbt` (SDK прошивки 1.5.1, API 88.2,
toolchain `arm-none-eabi-gcc` из `~/.ufbt`)

**Primary Dependencies**: только API прошивки — `furi`, `gui` (`view_dispatcher`,
`scene_manager`, модули `submenu`, `text_input`, `number_input`, `widget`, `file_browser`),
`dialogs`, `storage`, `notification`, `cli` (тестовая команда). Сторонних библиотек нет.

**Storage**: SD-карта, `STORAGE_APP_DATA_PATH_PREFIX` (`/ext/apps_data/tea_timer/`), файлы
`.tea` ≤ 1 КБ. Вызовы: `storage_file_open`, `storage_file_write`, `storage_file_close`,
`storage_common_remove`, `storage_common_rename`, `storage_common_mkdir`,
`storage_file_exists`, `storage_dir_exists` — все с отметкой `+` в
`targets/f7/api_symbols.csv` SDK 1.5.1 (сверено).

**Testing**: юнит-тесты **на устройстве** (принцип VIII): новый набор `preset_write` в
существующем раннере `unit_tests/`, запуск `tea_timer_test preset_write` через `ufbt cli`,
отчёт `N tests, M assertions, K failures` + `Leaked:` + `Status: PASSED|FAILED`. Дополнительно
`ufbt`, `ufbt format`, `ufbt lint` и ручные сценарии `quickstart.md`.

**Target Platform**: Flipper Zero, STM32WB55 (Cortex-M4F, 256 КБ RAM), экран 128×64,
1 бит/пиксель, прошивка 1.5.1.

**Project Type**: single project — одно FAP-приложение.

**Performance Goals**: открытие формы ≤ 200 мс; сохранение пресета (запись + обратное
чтение ≤ 1 КБ) ≤ 300 мс; удаление ≤ 200 мс; отклик на кнопку ≤ 100 мс; список файлов после
сохранения/удаления обновляется без перезапуска (перечитывается при входе в `FileSelect`).

**Constraints**: стек Main-потока 2 КБ (не увеличивается); черновик формы — в структуре
`TeaTimerApp`; буфер сериализации ≤ `TEA_FILE_MAX_SIZE + 1` в куче на время записи; в
GUI-потоке нет `furi_delay_ms` и блокирующего I/O; все ресурсы парные; строки UI —
английский ASCII.

**Scale/Scope**: 9 новых сцен, 6 новых ViewId (`Actions`, `EditName`, `EditNumber`,
`EditSummary`, `DeleteConfirm`, `Saved`), 3 новых модуля (`text_input`, `number_input` —
один экземпляр на все четыре числовых шага, `submenu` для действий) + 3 `widget`; 1 новый
набор тестов (~28 утверждений); до 100 пресетов в каталоге.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Проверка по конституции 3.0.1 (`.specify/memory/constitution.md`, принципы I–VIII).

| # | Гейт | Принцип | Статус | Обоснование |
|---|---|---|---|---|
| G1 | Только FAP, без правок прошивки | I | PASS | Все изменения — в каталоге приложения; `application.fam` не меняет `apptype=EXTERNAL`. |
| G2 | Только C и Furi API | II | PASS | Storage, GUI-модули, dialogs — публичный API; `snprintf` с границами; логи `FURI_LOG_*`. |
| G3 | Сначала готовые экраны прошивки | III | PASS | 9 новых экранов собраны из `submenu`, `text_input`, `number_input`, `widget`, `dialogs`; собственный `View` не добавляется. |
| G4 | `ufbt` — единственный toolchain | IV | PASS | `ufbt`, `ufbt launch`, `ufbt format`, `ufbt lint`, `ufbt cli`. |
| G5 | Ограниченная память, неблокирующий GUI | V | PASS | Черновик в `TeaTimerApp`; буфер записи в куче с парным `free`; запись ≤ 1 КБ не блокирует GUI заметно; ресурсы парные. |
| G6 | Скилл проекта обязателен | VI | PASS | План и research построены по `.opencode/skills/flipper-zero-app-dev` и заголовкам SDK 1.5.1. |
| G7 | Язык артефактов русский, кроме `README` | VII | PASS | Все артефакты на русском; английский — только строки UI и `README.md`. |
| G8 | Юнит-тесты на устройстве | VIII | PASS | Набор `preset_write` в `unit_tests/`, запуск CLI-командой на устройстве, отчёт с `Status`. |
| G9 | Тестовый код под отладочным флагом | VIII | PASS | Регистрация команды остаётся под `TEA_TIMER_TESTS`, как в 001; релизное поведение не меняется. |
| G10 | Английские ASCII-строки, 128×64 | разд. «Технические ограничения» | PASS | Все новые строки (`Edit`, `Delete`, `Saved`, `Deleted`, `Yes`, `No`, `Tea name`, `Leaf, 0.1 g/100ml`) короткие и латинские. |
| G11 | Формат `.tea` — публичный контракт | разд. «Технические ограничения» | PASS | Сериализация пишет ровно 5 существующих ключей в порядке 001; формат не меняется, обратное чтение — тем же парсером. |

**Нарушений нет.** Complexity Tracking не требуется: собственных `View` не добавляется,
новых потоков нет, формат файла не расширяется.

**Пост-дизайн проверка (после Phase 1)**: гейты G1–G11 перепроверены по `research.md`,
`data-model.md`, `contracts/*`, `quickstart.md`. Все PASS; единственное уточнение — запись
через временный файл (§R-W-2), что не меняет контракт формата, а снижает риск потери данных.

## Переиспользование стандартных экранов (принцип III)

Заголовки сверены с `~/.ufbt/current/sdk_headers/f7_sdk/...` (SDK 1.5.1) и
`targets/f7/api_symbols.csv`.

| # | Экран | Готовый компонент | Ключевые вызовы | Своего кода |
|---|---|---|---|---|
| 11 | Меню действий на карточке (`Edit`/`Delete`) | **`submenu`** | `submenu_reset`, `submenu_set_header`, `submenu_add_item`, `submenu_set_selected_item` | нет |
| 12 | Ввод названия (создание и правка) | **`text_input`** | `text_input_reset`, `text_input_set_header_text`, `text_input_set_result_callback`, `text_input_set_minimum_length(1)`, `text_input_set_validator` (свой валидатор допустимых символов) | нет |
| 13 | Граммовка | **`number_input`** | `number_input_set_header_text("Leaf, 0.1 g/100ml")`, `number_input_set_result_callback(..., 1, 500)` | нет |
| 14 | Первый пролив, с | **`number_input`** | `number_input_set_result_callback(..., 1, 3600)` | нет |
| 15 | Приращение, с | **`number_input`** | `number_input_set_result_callback(..., 0, 600)` | нет |
| 16 | Число проливов | **`number_input`** | `number_input_set_result_callback(..., 1, 99)` | нет |
| 17 | Сводка и подтверждение записи | **`widget`** + `widget_add_button_element` (`Save`) | `widget_add_string_element`, `widget_add_button_element(GuiButtonTypeCenter, "Save", ...)` | нет |
| 18 | Подтверждение удаления (`No` по умолчанию) | **`widget`** + две кнопки | `widget_add_string_element`, `widget_add_button_element(Center, "No", ...)`, `... (Right, "Yes", ...)` | нет |
| 19 | Пресет сохранён/удалён | **`widget`** + кнопка `OK` | `widget_add_string_element`, `widget_add_button_element(Center, "OK", ...)` | нет |
| 20 | Ошибки записи/удаления | **`dialogs`** `DialogMessage` | `dialog_message_alloc`, `dialog_message_set_header/text/buttons`, `dialog_message_show`, `dialog_message_free` | нет |

Не берём: `byte_input` (нет байтовых данных), `menu` (нужны иконки-ассеты),
`variable_item_list` (нет списка редактируемых значений), `text_box` (текст короткий),
блокирующий `dialog_file_browser_show` (ломает модель сцен).

## Project Structure

### Documentation (this feature)

```text
specs/002-preset-management/
├── plan.md                       # этот файл
├── spec.md                       # спецификация (после clarify)
├── research.md                   # Phase 0: решения и обоснования
├── data-model.md                 # Phase 1: сущности, состояния, валидация
├── quickstart.md                 # Phase 1: сборка, тесты на устройстве, приёмка
├── contracts/
│   ├── ui-screens.md             # контракт новых экранов 11–20 и переходов
│   └── file-write-delete.md      # контракт записи, правки и удаления файла
├── checklists/requirements.md
├── ui/                           # макеты (01–10)
└── tasks.md                      # Phase 2 (/speckit.tasks, создаётся не здесь)
```

### Source Code (repository root)

```text
tea_timer/
├── application.fam                    # без изменений (requires уже включают gui/storage/dialogs)
├── tea_timer.c                        # + аллокация/освобождение новых модулей и view
├── tea_timer.h                        # + PresetSaveResult/PresetDeleteResult, TEA_SLUG_MAX
├── tea_timer_i.h                      # + 6 ViewId, TeaPreset draft, bool editing, EditStep step
├── tea_timer_custom_event.h           # + события формы/действий/удаления
├── scenes/
│   ├── tea_timer_scene.h              # + TeaTimerSceneActions … TeaTimerSceneSaved
│   ├── tea_timer_scene.c              # + обработчики в трёх массивах
│   ├── tea_timer_scene_menu.c         # + пункт New preset
│   ├── tea_timer_scene_preset_card.c  # + кнопка Right ">" → Actions
│   ├── tea_timer_scene_file_select.c  # без изменений (перечитывает каталог при входе)
│   ├── tea_timer_scene_actions.c      # NEW: submenu Edit/Delete
│   ├── tea_timer_scene_edit_name.c    # NEW: text_input + валидатор
│   ├── tea_timer_scene_edit_leaf.c    # NEW: number_input 1..500
│   ├── tea_timer_scene_edit_first.c   # NEW: number_input 1..3600
│   ├── tea_timer_scene_edit_increment.c # NEW: number_input 0..600
│   ├── tea_timer_scene_edit_max_pours.c # NEW: number_input 1..99
│   ├── tea_timer_scene_edit_summary.c # NEW: widget + Save → запись
│   ├── tea_timer_scene_delete_confirm.c # NEW: widget No/Yes → удаление
│   ├── tea_timer_scene_saved.c        # NEW: widget Saved/Deleted → FileSelect
│   └── tea_timer_scene_file_select.c  # + tea_timer_goto_file_select() (R-N-1)
├── lib/tea_logic/
│   ├── tea_preset.c/.h                # без изменений (парсер 001)
│   ├── tea_preset_load.c/.h           # без изменений
│   ├── tea_preset_save.c              # NEW: serialize, slug, unique path, save, delete
│   ├── tea_preset_save.h              # NEW
│   └── brew_math.c/.h                 # + brew_session_clear()
├── unit_tests/
│   ├── test_index.c                   # + регистрация набора preset_write
│   └── lib_tests/preset_write/preset_write_test.c  # NEW
├── images/, views/                    # без изменений
└── README.md
```

`dist/tea_timer.fap` — результат `ufbt`.

**Structure Decision**: структура 001 сохраняется. Новые сцены живут в `scenes/`, логика
записи/удаления — в приватной библиотеке `lib/tea_logic` (ufbt исключает `lib/` из
автосбора, поэтому модуль обязан быть в `fap_private_libs`, как в 001). Собственных
`View` не появляется.

## Владение ресурсами

Добавляется к порядку из 001 (без изменения существующего):

`tea_timer_alloc()` дополнительно:
1. `actions_menu = submenu_alloc()` → `view_dispatcher_add_view(TeaTimerViewActions, ...)`.
2. `text_input = text_input_alloc()` → `TeaTimerViewEditName`; `text_input_set_validator(...)`.
3. `number_input = number_input_alloc()` → `TeaTimerViewEditNumber` (один экземпляр на все
   четыре числовых шага; сцена перед показом меняет заголовок, границы и callback).
4. `edit_summary = widget_alloc()` → `TeaTimerViewEditSummary`.
5. `delete_confirm = widget_alloc()` → `TeaTimerViewDeleteConfirm`.
6. `saved = widget_alloc()` → `TeaTimerViewSaved`.
7. `draft` — статическое поле в `TeaTimerApp` (без аллокаций).

`tea_timer_free()` — строго обратный порядок: `remove_view` для каждого нового view →
`*_free` модулей → далее существующий порядок 001. Буфер сериализации выделяется на время
одной операции `tea_preset_save()` и освобождается на всех путях, включая ошибки.

## Тестирование на устройстве (принцип VIII)

Раннер, CLI-команда и отчёт — существующие из 001; добавляется набор и несколько чистых
функций, чтобы проверять их без GUI.

| Набор | Покрытие |
|---|---|
| `preset_write` (новый) | `tea_preset_build_slug`: `Green Tea` → `green_tea`, регистр, пробелы, несколько пробелов, пустое имя → `preset`, 24 символа, недопустимые символы вырезаются; `tea_preset_make_path`: базовое имя, суффиксы `_2`/`_3`, граница 99; `tea_preset_serialize`: точный текст 5 полей с `\n`, длина ≤ 1024, обратный разбор `tea_preset_parse_buf` даёт те же значения; `tea_preset_check_draft`: пустое имя, 25 символов, запрещённые символы, границы 0/1/50.0/50.1, 0/1/3600/3601, 0/600/601, 0/1/99/100; `tea_preset_save`: создание файла во временном каталоге `APP_DATA_PATH("test")`, совпадение имени → суффикс, правка того же пути (имя не меняется), обратное чтение совпадает, удаление временных файлов после теста; `tea_preset_delete`: удаление существующего файла, `ErrNotFound` на отсутствующем, повторное удаление; `brew_session_clear`: `has_preset=false`, счётчик 0, фаза `IDLE` |

Тесты работают во временном подкаталоге `APP_DATA_PATH("test")` и удаляют за собой файлы и
каталог; состояние реальных пресетов пользователя не затрагивается. Детерминизм: без
зависимости от реального времени и порядка запуска.

## Порядок реализации (крупные шаги)

1. Модель и чистая логика: `tea_preset_save.[ch]` (`build_slug`, `make_path`,
   `serialize`, `check_draft`), `brew_session_clear()`; расширить `tea_timer.h`.
2. Набор `preset_write` + регистрация в `test_index.c`; прогон на устройстве —
   `Status: PASSED` до появления UI.
3. Сцены формы: `edit_name` → `edit_leaf` → `edit_first` → `edit_increment` →
   `edit_max_pours` → `edit_summary`; пункт `New preset` в меню.
4. Запись из сводки: `tea_preset_save()` (temp + verify + rename), экран `Saved`,
   возврат к выбору файла.
5. Меню действий на карточке (`>`), сцена `actions`, правка существующего пресета тем же
   путём (запись в тот же файл).
6. Удаление: `delete_confirm` (No/Yes), `tea_preset_delete()`, сброс сессии удалённого
   пресета, экран `Deleted`, возврат к выбору файла.
7. Ошибки записи/удаления (`DialogMessage`), граничные сценарии из `quickstart.md`.
8. `ufbt`, `ufbt format`, `ufbt lint`, полный прогон `tea_timer_test` на устройстве,
   проверка `free`/`top` до и после.

## Риски и смягчение

| Риск | Смягчение |
|---|---|
| Потеря существующего файла при неудачной правке | Запись во временный файл, обратное чтение, только затем подмена целевого (R-W-2) |
| Долгая запись блокирует GUI | Объём ≤ 1 КБ, одна операция по нажатию; при необходимости — `loading` из 001 |
| Расхождение `number_input` (целые) и граммовки (десятые) | Вводятся десятые доли, заголовок `Leaf, 0.1 g/100ml`; в файл пишется `%u.%u` |
| Стек/куча после добавления модулей | Проверка `free`/`top` по `quickstart.md`; при нехватке — освобождение модулей после выхода из сценария (не требуется по расчёту ~1.2 КБ) |
| `storage_common_rename` не перезаписывает существующий файл | Целевой файл удаляется перед `rename` только после успешной проверки временного; ошибка `rename` → сообщение, временный файл остаётся |

## Complexity Tracking

Нарушений конституции нет; таблица не требуется.
