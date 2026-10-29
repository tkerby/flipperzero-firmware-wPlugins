# Контракт UI — экраны, компоненты API, входы и переходы

Контракт фиксирует: **какой готовый компонент прошивки** используется на каждом экране,
какие входы он получает, какое событие отдаёт и куда ведёт переход. Строки — только
английские (FR-021), кириллица запрещена.

Проверено по SDK 1.4.3 (`~/.ufbt/current/sdk_headers/f7_sdk/applications/services/gui/`).

---

## C-UI-0. Общие правила

| Правило | Значение |
|---|---|
| Тип ViewDispatcher | `ViewDispatcherTypeFullscreen` (128×64, без статус-бара) |
| Навигация | `SceneManager`; BACK в корневой сцене → выход из приложения |
| Тик | `view_dispatcher_set_tick_event_callback(vd, cb, 1000)` — 1 Гц |
| Подсказки кнопок | на каждом экране (FR-022): текст вида `OK: start`, `BACK: back` или `widget_add_button_element` |
| Обрезка длинного текста | `elements_string_fit_width` (Canvas) / предварительное `snprintf` с ограничением |
| Блокирующие вызовы в GUI-потоке | запрещены (кроме `dialog_message_show*`, вызываемых из сцены до/после переключения view) |

---

## C-UI-1. Scene `Menu` — главное меню (002 FR-001)

| Аспект | Значение |
|---|---|
| Компонент | `submenu` (`gui/modules/submenu.h`) |
| Создание | `submenu_alloc()` один раз в `tea_timer_alloc` |
| Настройка в `on_enter` | `submenu_reset()`, `submenu_set_header(sm, "Tea Timer")`, `submenu_add_item(sm, "Presets", TeaTimerMenuIndexPresets, cb, app)`, `submenu_add_item(sm, "New preset", TeaTimerMenuIndexNewPreset, cb, app)` |
| ViewId | `TeaTimerViewMenu` |
| Вход | Up/Down — выделение; OK — пункт; BACK — выход из приложения |
| Callback | `submenu_add_item` → прямой `scene_manager_next_scene(sm, TeaTimerSceneFileSelect)` (компонент уже в GUI-потоке; отдельное событие `MenuSelected` не нужно) |
| Переходы | `Presets` → `SceneFileSelect`; `New preset` → `SceneEditName` (002) |

---

## C-UI-2. Scene `FileSelect` — системный выбор файла (FR-002/003/004 из 002)

| Аспект | Значение |
|---|---|
| Компонент | `file_browser` (`gui/modules/file_browser.h`) |
| Создание | `app->file_path = furi_string_alloc_set(APP_DATA_PATH(""))` в `alloc`; `app->file_browser = file_browser_alloc(app->file_path)` |
| Конфигурация (один раз) | `file_browser_configure(app->file_browser, ".tea", APP_DATA_PATH(""), false, true, &I_tea_file, true)` — только `.tea`, скрывать dot-файлы, скрывать расширение в списке |
| `on_enter` | `file_browser_start(app->file_browser, furi_string_alloc_set(APP_DATA_PATH("")))` (путь сбрасывается в корень каталога приложения) |
| `on_exit` | `file_browser_stop(app->file_browser)` |
| Callback | `file_browser_set_callback(fb, tea_timer_file_browser_cb, app)` → `view_dispatcher_send_custom_event(vd, TeaTimerEventFileSelected)` |
| ViewId | `TeaTimerViewFileSelect` |
| Вход | Up/Down — навигация, Right/OK — вход/выбор, Left/BACK — вверх по каталогу; BACK в корне каталога → `SceneMenu` |
| Выход | `app->file_path` содержит выбранный путь |
| Переходы | успешный парсинг → `ScenePresetCard`; ошибка → диалог (C-UI-10) и остаёмся в `FileSelect` |

Ограничение: навигация не выходит за `APP_DATA_PATH("")` (001 FR-001) — обеспечивается
параметром `base_path`.

---

## C-UI-3. Scene `PresetCard` — карточка пресета (001 FR-009)

| Аспект | Значение |
|---|---|
| Компонент | `widget` (`gui/modules/widget.h`) |
| `on_enter` | `widget_reset(w)`; `widget_add_string_element(w, 64, 2, AlignCenter, AlignTop, FontPrimary, preset.name)`; `widget_add_string_element(w, 64, 18, AlignCenter, AlignTop, FontSecondary, "Leaf 5.0 g/100ml")`; `widget_add_string_element(w, 64, 30, ..., "Pours 0 / 6")`; `widget_add_string_element(w, 64, 42, ..., "Pour 1  00:20")`; `widget_add_button_element(w, GuiButtonTypeCenter, "Start", cb, app)` |
| ViewId | `TeaTimerViewPresetCard` |
| Вход | OK/Center — `Start`; BACK — `SceneFileSelect` |
| Переход | `Start` → `SceneBrew`; `brew_session_start(furi_get_tick(), &app->session)` выполняется по `TeaTimerEventBrewStart` в сцене `Brew`; сброс сессии — только при выборе другого файла (FR-016) |
| Примечание | карточка и есть экран заваривания в ожидании (`IDLE`): отдельного промежуточного экрана нет, поэтому `Start` сразу запускает отсчёт (SC-001 — 3 нажатия) |

---

## C-UI-4. Scene `Brew` — экран заваривания (FR-009…FR-016, FR-018)

| Аспект | Значение |
|---|---|
| Компонент | **собственный** `View` `brew_view` (`views/brew_view.c`), только он рисуется с нуля |
| Модель | `BrewViewModel`, `ViewModelTypeLockFree` |
| ViewId | `TeaTimerViewBrew` |
| `on_enter` | `brew_view_update(app->brew_view, &app->session)`, `view_dispatcher_switch_to_view(vd, TeaTimerViewBrew)` |
| `on_event` Custom | `TeaTimerEventTick` → пересчёт `remaining_sec`; при `0` — `brew_session_complete()` + сигнал + `with_view_model`; `TeaTimerEventBrewStart` → `brew_session_start()`; `TeaTimerEventBrewStop` → `brew_session_abort()` + `scene_manager_previous_scene()` |
| `on_event` Back | остановить отсчёт (`brew_session_abort()`), `scene_manager_previous_scene()` — счётчик **не** сбрасывается (FR-016) |
| Вход OK | `draw`/`input`-callback `brew_view` не меняет сессию, а отправляет `TeaTimerEventBrewStart` (не `RUNNING`) или `TeaTimerEventBrewStop` (`RUNNING`) — состояние меняет только сцена |
| Вход Back | как `on_event` Back |
| Layout `IDLE` | не рисуется: ожидание показывает карточка `PresetCard` (C-UI-3) |
| Layout `RUNNING` | `name`, остаток `mm:ss`/`hh:mm:ss` шрифтом `FontBigNumbers` по центру, `Pours X / N`, подсказка `OK: stop` |
| Layout `DONE` | `name`, инверсный баннер `Time is up!`, `Pours X / N`, `Pour N+1  mm:ss`, подсказка `OK: start next pour` |
| Признак `OVER` | при `completed_pours > max_pours`: счётчик в инверсном блоке + плашка `OVER`; отсчёт запускается без диалогов (FR-014) |
| Сигнал | один раз при `RUNNING → DONE` (FR-019), повторно не звучит в `DONE` |
| Экран | `sequence_display_backlight_enforce_on` в `RUNNING`, `..._auto` при выходе из `RUNNING` (FR-017) |

---

## C-UI-5. Scene `NoPresets` — каталог пуст (FR-008)

| Аспект | Значение |
|---|---|
| Компонент | `widget` |
| `on_enter` | `widget_reset`; `widget_add_string_element(w, 64, 12, AlignCenter, AlignTop, FontPrimary, "No presets found")`; `widget_add_string_element(w, 64, 28, ..., FontSecondary, "Add .tea files to")`; `widget_add_string_element(w, 64, 40, ..., FontSecondary, "apps_data/tea_timer")`; `widget_add_button_element(w, GuiButtonTypeCenter, "Reload", cb, app)` |
| Вход | OK/Center — повторная проверка каталога; BACK — выход/меню |
| Переход | при появлении файлов → `SceneFileSelect`; иначе остаёмся (или `popup` «Still empty») |
| Условие показа | `!storage_dir_exists(APP_DATA_PATH(""))` или в каталоге нет ни одного `.tea` |

---

## C-UI-6. Scene `Error` — SD-карта недоступна / ошибка чтения

| Аспект | Значение |
|---|---|
| Компонент | `dialogs` (`dialog_message_show_storage_error`) или `widget` с путём и `Retry` |
| Условие | `storage_sd_status(app->storage) != StorageStatusOK` при входе, либо `storage_file_open` вернул ошибку |
| Текст | `SD card not found` + `Insert the card and press OK` (FR-024), без кириллицы |
| Вход | OK — повторная попытка (re-read); BACK — выход |
| Переход | успех → `SceneFileSelect`; повторная ошибка → остаёмся |

---

## C-UI-7. Scene `Skipped` — файл пропущен (не реализуется в 001)

| Аспект | Значение |
|---|---|
| Компонент | `popup` (с таймаутом и/или OK) |
| Настройка | `popup_reset`; `popup_set_header(p, "Skipped 1 file", 64, 2, AlignCenter, AlignTop)`; `popup_set_text(p, "invalid values in\nbad.tea", 64, 20, AlignCenter, AlignTop)`; `popup_set_icon(p, 0, 0, &I_WarningDolphin_45x26)` (стандартная иконка прошивки) |
| Вход | OK — закрыть; таймаут 3000 мс — авто-закрытие |
| Переход | возврат на предыдущий экран (список/браузер) |
| Примечание | в 001 счётчика пропущенных файлов нет: причина отказа показывается диалогом C-UI-10 (FR-007), эта сцена в 001 не используется |

---

## C-UI-8. Диалог «нет SD-карты» (системный)

Отдельная сцена не нужна, если используется блокирующий `dialogs` API:
`furi_record_open(RECORD_DIALOGS)` → `dialog_message_show_storage_error(dialogs, "SD card not found")`
→ `furi_record_close`. Вызывается из `SceneFileSelect.on_enter` **до** `switch_to_view`,
поэтому GUI-поток не блокируется «внутри отрисовки».

---

## C-UI-9. Формы создания пресета (002 FR-002…FR-010) — готовые вводы

| Шаг | Компонент | Настройка | Валидация | Переход |
|---|---|---|---|---|
| 1. Название | `text_input` | `text_input_set_header_text(ti, "Tea name")`; `text_input_set_result_callback(ti, cb, app, app->edit.name, sizeof(app->edit.name), false)`; `text_input_set_minimum_length(ti, 1)`; `text_input_set_validator(ti, validator, app)` | 1–24 символа, латиница/цифры/пробел/`_` | OK → шаг 2; BACK → `Menu` (без файла) |
| 2. Граммовка | `number_input` | `number_input_set_header_text(ni, "Leaf, 0.1 g/100ml")`; `number_input_set_result_callback(ni, cb, app, &app->edit.leaf_tenths, 1, 500)` | 1–500 | OK → шаг 3 |
| 3. Первый пролив, с | `number_input` | `number_input_set_result_callback(ni, cb, app, &app->edit.first_pour, 1, 3600)` | 1–3600 | OK → шаг 4 |
| 4. Приращение, с | `number_input` | `... (&app->edit.increment, 0, 600)` | 0–600 | OK → шаг 5 |
| 5. Число проливов | `number_input` | `... (&app->edit.max_pours, 1, 99)` | 1–99 | OK → шаг 6 |
| 6. Сводка | `widget` + `DialogMessage` | `widget_add_string_*` со всеми значениями; `dialog_message_set_buttons(msg, "Edit", NULL, "Save")` | — | `Save` → запись файла; `Edit` → шаг 1 |
| 7. Результат | `popup` | `popup_set_header(p, "Saved", ...)`, `popup_set_text(p, "green_tea.tea", ...)` | — | OK → `Menu`/`FileSelect` |

Предзаполнение (002 FR-005): 5.0 г → `leaf_tenths = 50`, 20 с, 5 с, 6 проливов.

---

## C-UI-10. Диалог ошибки выбранного файла (001 FR-007, 002 FR-021)

| Аспект | Значение |
|---|---|
| Компонент | `DialogMessage` (`dialogs`) |
| Настройка | `dialog_message_alloc()`; `dialog_message_set_header(msg, "Cannot use file", ...)`; `dialog_message_set_text(msg, "Missing field: max_pours", ...)`; `dialog_message_set_buttons(msg, "Retry", NULL, "OK")` |
| Показ | `dialog_message_show(dialogs, msg)` → `DialogMessageButtonLeft` (Retry) / `Right` (OK) |
| Переход | `Retry` → снова `SceneFileSelect`; `OK` → `SceneFileSelect` с сохранённой позицией (файл остаётся виден) |
| Тексты | из таблицы `PresetError` в [data-model.md](../data-model.md#5-presetscanresult--preseterror--ошибки-чтения); только короткие английские строки |

---

## Карта сцен

```text
        ┌──────────────────────────┐
        │ Menu (submenu)           │
        │  Presets / New preset    │
        └───┬──────────────────┬───┘
   Presets  │                  │ New preset
            ▼                  ▼
 ┌──────────────────┐   ┌─────────────────────────────┐
 │ FileSelect       │   │ EditName (text_input)       │
 │ (file_browser)   │   └───┬─────────────────────────┘
 └──┬────────────┬──┘       ▼
    │ файл OK    │ ошибка   EditLeaf (number_input) → EditFirst →
    ▼            ▼          EditIncrement → EditMaxPours →
┌────────────┐ ┌──────────────┐   Summary (widget + DialogMessage) → Saved (popup)
│ PresetCard │ │ Error dialog │        │
│ (widget)   │ │  Retry / OK  │        └→ Menu / FileSelect
└─────┬──────┘ └──────────────┘
      │ Start
      ▼
┌──────────────────────────────────────────────┐
│ Brew (brew_view, Canvas)                     │
│  IDLE ⇄ RUNNING → DONE (сигнал, +1 pour)     │
│  OVER-плашка при completed > max             │
└─────┬────────────────────────────────────────┘
      │ BACK (сессия сохраняется)
      ▼
   Menu / FileSelect
```

Экраны `NoPresets` (каталог пуст) и `SD error` вызываются из `FileSelect`/`Menu` до
показа списка файлов.
