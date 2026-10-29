# Контракт экранов — создание, правка и удаление пресета (002)

Дополняет [`001/contracts/ui-screens.md`](../../001-tea-brew-timer/contracts/ui-screens.md).
Экраны 01–10 из 001 не переделываются, кроме одного изменения: на карточке пресета
появляется кнопка `>` (`C-UI-3`, п. «Дополнение 002»).

Общие правила: строки — английский ASCII ≤ 21 символа мелким шрифтом; на каждом экране
есть подсказка кнопок; BACK никогда не приводит к записи или удалению файла.

---

## C-UI-3 (дополнение). Карточка пресета — кнопка действий

| Аспект | Значение |
|---|---|
| Компонент | существующий `widget` (`tea_timer_scene_preset_card.c`) |
| Дополнение | `widget_add_button_element(w, GuiButtonTypeRight, ">", cb, app)` |
| Переход | `>` → `TeaTimerSceneActions`; `Start` (Center) и BACK — без изменений |
| Инвариант | кнопка действий доступна только когда пресет загружен (`session.has_preset`) |

---

## C-UI-11. Scene `Actions` — меню действий

| Аспект | Значение |
|---|---|
| Компонент | `submenu` (`gui/modules/submenu.h`), отдельный экземпляр `app->actions_menu` |
| `on_enter` | `submenu_reset`; `submenu_set_header(submenu, preset.name)`; `submenu_add_item("Edit", 0, cb)`, `submenu_add_item("Delete", 1, cb)`; `submenu_set_selected_item(submenu, 0)` |
| ViewId | `TeaTimerViewActions` |
| Вход | кнопка `>` на карточке |
| `Edit` | `editing=true`; `draft = session.preset`; `TeaTimerSceneEditName` |
| `Delete` | `TeaTimerSceneDeleteConfirm` |
| BACK | `scene_manager_previous_scene` → карточка; файл не меняется |
| Подсказка | пункты меню; собственных кнопок нет (стандартный submenu) |

---

## C-UI-12. Scene `EditName` — ввод названия

| Аспект | Значение |
|---|---|
| Компонент | `text_input` (`app->text_input`) |
| `on_enter` | `text_input_reset`; `text_input_set_header_text("Tea name")`; `text_input_set_result_callback(ti, cb, app, app->draft.name, sizeof(app->draft.name), false)`; `text_input_set_minimum_length(ti, 1)`; `text_input_set_validator(ti, tea_timer_edit_name_validator, app)` |
| Валидатор | допустимы `A–Z a–z 0–9`, пробел и `_`; иначе `error` = `A-Z a-z 0-9 _`; текст длиннее 24 байт невозможен (буфер 25) |
| ViewId | `TeaTimerViewEditName` |
| Вход | `New preset` (создание) / `Edit` (правка) |
| OK | `TeaTimerSceneEditNumber` с шагом `Leaf` |
| BACK | создание → `Menu`; правка → `Actions`; файл не создаётся/не меняется |
| Значение по умолчанию | создание — пустая строка; правка — `draft.name` |

---

## C-UI-13. Scene `EditNumber` — числовые шаги (один экземпляр)

| Аспект | Значение |
|---|---|
| Компонент | `number_input` (`app->number_input`), переиспользуется четырьмя шагами |
| ViewId | `TeaTimerViewEditNumber` |
| Настройка перед показом | `number_input_set_header_text`, `number_input_set_result_callback(ni, cb, app, &draft.<field>, min, max)` |

| `step` | Заголовок | Поле | min | max | Единица |
|---|---|---|---|---|---|
| `Leaf` | `Leaf, 0.1 g/100ml` | `draft.leaf_tenths_g` | 1 | 500 | 0.1 г |
| `First` | `First pour, sec` | `draft.first_pour_sec` | 1 | 3600 | с |
| `Increment` | `Step, sec` | `draft.increment_sec` | 0 | 600 | с |
| `MaxPours` | `Max pours` | `draft.max_pours` | 1 | 99 | шт |

| Событие | Поведение |
|---|---|
| OK | следующий шаг: `Leaf → First → Increment → MaxPours → Summary` |
| BACK | предыдущий шаг; с `Leaf` — обратно на `EditName` |
| Начальное значение | создание — умолчания (50 десятых, 20, 5, 6); правка — из `draft` |

---

## C-UI-14. Scene `EditSummary` — сводка и запись

| Аспект | Значение |
|---|---|
| Компонент | `widget` (`app->edit_summary`) |
| `on_enter` | `widget_reset`; заголовок `New preset` или `Edit preset`; имя (обрезка по ширине); `Leaf %u.%u g/100ml`; `First MM:SS  Step MM:SS`; `Max %u pours`; `widget_add_button_element(Center, "Save", cb)` |
| ViewId | `TeaTimerViewEditSummary` |
| `Save` | `tea_preset_check_draft()` → при ошибке диалог (C-UI-20) и возврат на проблемный шаг; при успехе `tea_preset_save()` |
| Успех | `TeaTimerSceneSaved` с типом `Saved` и именем файла |
| Ошибка записи | диалог `Cannot save file` с `Retry`/`Cancel`; введённые значения сохраняются в `draft` |
| BACK | предыдущий шаг (`MaxPours`); файл не пишется |
| Подсказка | `Save` на кнопке Center |

---

## C-UI-15. Scene `DeleteConfirm` — подтверждение удаления

| Аспект | Значение |
|---|---|
| Компонент | `widget` (`app->delete_confirm`) |
| `on_enter` | строка 1 — `preset.name` (обрезка многоточием); строка 2 — basename файла (обрезка); строка 3 — `Delete preset?`; кнопки `GuiButtonTypeCenter = "No"`, `GuiButtonTypeRight = "Yes"` |
| ViewId | `TeaTimerViewDeleteConfirm` |
| `No` (Center) | возврат на карточку, файл не меняется |
| `Yes` (Right) | `tea_preset_delete()`; успех → `Saved` с типом `Deleted`; ошибка → диалог C-UI-20 (`Retry`/`Cancel`) |
| BACK | равно `No` |
| Инвариант | удаление невозможно без явного нажатия `Yes`; OK по умолчанию отменяет |

---

## C-UI-16. Scene `Saved` — результат операции

| Аспект | Значение |
|---|---|
| Компонент | `widget` (`app->saved`) |
| `on_enter` | заголовок `Saved` или `Deleted`; имя файла; кнопка Center `OK` |
| ViewId | `TeaTimerViewSaved` |
| `OK` | `tea_timer_goto_file_select(app)` (R-N-1); при пустом каталоге — маршрутизация `tea_timer_show_presets` даёт `NoPresets` |
| BACK | то же, что `OK` (не оставляем пользователя в тупике) |

---

## C-UI-17. Диалоги ошибок (C-UI-20)

| Ситуация | Заголовок | Текст | Кнопки | Результат |
|---|---|---|---|---|
| Ошибка проверки черновика | `Bad value` | `%s` (поле + диапазон) | `Retry` | возврат на проблемный шаг |
| `.tmp` не открылся / запись не удалась | `Cannot save` | `Check the SD card` | `Retry`, `Cancel` | повтор или сводка |
| Правка: подмена файла не удалась | `Cannot save` | `File is kept` | `OK` | остаёмся на сводке |
| Удаление: нет файла | `Cannot delete` | `File not found` | `OK` | возврат на карточку; список обновится |
| Удаление: ошибка карты | `Cannot delete` | `Check the SD card` | `Retry`, `Cancel` | повтор или карточка |
| Нет SD-карты | `SD card not found` | `Insert the card` | `OK` | назад на карточку |

Все диалоги — `DialogMessage` (`dialogs`), как в 001 C-UI-10.

---

## Карта переходов (002)

```text
Menu ──New preset──► EditName ──► EditNumber(Leaf) ──► (First) ──► (Increment) ──► (MaxPours)
                          ▲                │ BACK = предыдущий шаг                    │
                          └────────────────┴──────────────────────────────────────────┘
                                                                                       ▼
                                                                                  EditSummary
                                                                                       │ Save
                                                                                       ▼
                                                                              Saved ──OK──► FileSelect
Card ──>──► Actions ──Edit────► EditName (текущие значения) ──► … ──► EditSummary ──► Saved
                 └──Delete──► DeleteConfirm ──No/BACK──► Card
                                   └──Yes──► удаление ──► Saved(Deleted) ──OK──► FileSelect
```

## Управление

| Экран | Up/Down | Left/Right | OK | Back |
|---|---|---|---|---|
| Actions | выбор пункта | — | открыть пункт | карточка |
| EditName | клавиатура | клавиатура | подтвердить | Menu / Actions |
| EditNumber | клавиатура цифр | клавиатура цифр | следующий шаг | предыдущий шаг |
| EditSummary | — | — | `Save` | предыдущий шаг |
| DeleteConfirm | — | `Yes` (право) | `No` (центр) | `No` |
| Saved | — | — | `OK` | `OK` |
