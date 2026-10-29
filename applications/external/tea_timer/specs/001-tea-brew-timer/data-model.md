# Data Model — Tea Timer

Сущности выводятся из `spec.md` (Key Entities + FR-004/FR-009…FR-018) и учитывают
замену FR-006 спецификацией `002-preset-management` (системный выбор файла, создание
пресетов на устройстве).

Формат хранения — см. [contracts/tea-file-format.md](./contracts/tea-file-format.md);
каталог и имена файлов — [contracts/storage-layout.md](./contracts/storage-layout.md).

---

## 1. TeaPreset — пресет чая

Один `.tea`-файл = один пресет. Загружается в память при выборе файла, живёт до выбора
другого пресета или выхода из приложения.

| Поле | Тип | Диапазон / правило | Источник |
|---|---|---|---|
| `name` | `char name[TEA_NAME_MAX + 1]` (25) | 1–24 байта, латиница/цифры/пробел/`_`, печатаемые ASCII, не пусто | FR-004, 002 FR-003 |
| `leaf_tenths_g` | `uint16_t` | 1–500 (десятые доли грамма: 0.1–50.0 г) | FR-004 |
| `first_pour_sec` | `uint16_t` | 1–3600 | FR-004 |
| `increment_sec` | `uint16_t` | 0–600 | FR-004 |
| `max_pours` | `uint8_t` | 1–99 | FR-004 |
| `source_path` | `FuriString*` / `char path[TEA_PATH_MAX]` | путь к файлу на SD, только чтение; нужен для 002 FR-012 (правка «в тот же файл») | FR-006 (заменён), 002 FR-012 |

**Инварианты**:

- все пять полей обязательны и встречаются ровно один раз (дубликат ключа → файл некорректен);
- любое значение вне диапазона → файл некорректен целиком (FR-004, Edge Cases);
- `leaf_tenths_g` хранится целым, в UI выводится как `%u.%u` (`5.0`, `0.1`, `50.0`) — без
  float в модели;
- неизвестные ключи игнорируются (Assumptions);
- источник — только чтение для 001; запись появляется в 002 (сериализация того же формата).

**Деривативные (не хранятся)**: `leaf_g_text` (строка для UI), `pour_sec(n)` (см. §3).

---

## 2. BrewSession — сессия заваривания

Состояние работы с выбранным пресетом. Живёт в `TeaTimerApp` весь запуск приложения;
при выборе другого пресета сбрасывается; при выходе не сохраняется (Assumptions).

| Поле | Тип | Правило |
|---|---|---|
| `has_preset` | `bool` | true — пресет выбран и сессия активна |
| `preset` | `TeaPreset` | копия загруженного пресета |
| `completed_pours` | `uint16_t` | число успешно завершённых проливов; 0 в начале сессии (FR-012) |
| `next_pour_sec` | `uint16_t` | длительность следующего пролива; 1-й = `first_pour_sec`, далее + `increment_sec` (FR-011) |
| `phase` | `BrewPhase` | `IDLE` / `RUNNING` / `DONE` (см. §3) |
| `deadline_tick` | `uint32_t` | `furi_get_tick() + next_pour_sec*1000` на момент старта (только для `RUNNING`) |
| `remaining_sec` | `uint16_t` | производное: `(deadline_tick - furi_get_tick() + 999) / 1000` |
| `over_limit` | `bool` | производное: `completed_pours > preset.max_pours` (FR-014) |

**Инварианты**:

- `completed_pours` инкрементируется **только** при переходе `RUNNING → DONE` (FR-012);
- `next_pour_sec` пересчитывается **только** там же, по правилу `prev + increment_sec`;
- прерывание (`RUNNING → IDLE` по OK/BACK) не меняет ни `completed_pours`, ни `next_pour_sec`
  (FR-015, Edge Cases: остаток не сохраняется);
- возврат в меню по BACK сохраняет сессию; повторное открытие того же пресета продолжает
  серию и не запускает отсчёт автоматически (FR-016);
- при предельных значениях `next_pour_sec` не превышает 3600 + 99·600 = 63000 с —
  укладывается в `uint32_t`, формат `hh:mm:ss` (FR-018).

---

## 3. BrewPhase — состояние таймера

```text
        ┌──────────────── OK (START) ───────────────┐
        │                                           ▼
   ┌─────────┐  время истекло: сигнал, +1, next+inc  ┌───────────┐
   │  IDLE   │◄──────── BACK / другой пресет ───────│  RUNNING  │
   └─────────┘                                      └───────────┘
        ▲                                              │      │
        │                                    OK (STOP)│      │ время истекло
        │                                              ▼      ▼
        │                                          ┌───────────────┐
        └──────────── OK / BACK ───────────────────│     DONE      │
                                                   └───────────────┘
                                                     (Time is up!,
                                                      без автозапуска)
```

| Переход | Условие | Побочные эффекты |
|---|---|---|
| `IDLE → RUNNING` | OK на экране заваривания, `!RUNNING` | `deadline_tick` = now + `next_pour_sec`; backlight `enforce_on`; повторный OK игнорируется (Edge Cases) |
| `RUNNING → DONE` | `furi_get_tick() >= deadline_tick` (по тику 1 Гц) | однократный сигнал+вибро; `completed_pours++`; `next_pour_sec += increment_sec`; backlight `enforce_auto` |
| `RUNNING → IDLE` | OK (STOP) | без сигнала, счётчик и время не меняются; backlight `enforce_auto` |
| `DONE → RUNNING` | OK | как `IDLE → RUNNING`, но с новым `next_pour_sec` |
| `DONE → IDLE` | — | не требуется: `DONE` остаётся до OK/BACK (FR-013) |
| любой → `IDLE` | BACK в меню / выбор другого пресета | таймер остановлен, backlight авто; сессия сохраняется при BACK и сбрасывается при другом пресете |

Повторный сигнал в `DONE` невозможен: событие «время истекло» обрабатывается один раз
(флаг перехода), тик в `DONE` только перерисовывает.

---

## 4. BrewViewModel — модель экрана (только представление)

`ViewModelTypeLockFree`, копия подмножества сессии для отрисовки в GUI-потоке.

| Поле | Тип | Назначение |
|---|---|---|
| `phase` | `uint8_t` (`BrewPhase`) | выбор layout |
| `name` | `char[25]` | название чая, обрезается по ширине |
| `completed_pours` | `uint16_t` | `Pours X / N` |
| `max_pours` | `uint8_t` | знаменатель |
| `over_limit` | `bool` | плашка `OVER`, инверсия счётчика (не отдельная фаза: фаз три — `IDLE`/`RUNNING`/`DONE`; `IDLE` рисует карточка) |
| `leaf_tenths_g` | `uint16_t` | `Leaf N.N g/100ml` |
| `pour_number` | `uint16_t` | `completed_pours + 1` |
| `pour_sec` | `uint16_t` | длительность следующего пролива |
| `remaining_sec` | `uint16_t` | крупный таймер в `RUNNING` |

Инвариант: модель обновляется только через `with_view_model(..., true)` из GUI-потока
(тик-callback/сцены); в `draw_callback` модель не мутируется.

---

## 5. PresetScanResult / PresetError — ошибки чтения

Используется диалогами «файл пропущен» (FR-007) и ошибкой валидации (001 FR-007).

| Поле | Тип | Назначение |
|---|---|---|
| `result` | `TeaPresetResult` | `OK`, `ERR_NOT_FOUND`, `ERR_TOO_LARGE`, `ERR_NO_FIELD`, `ERR_BAD_NUMBER`, `ERR_OUT_OF_RANGE`, `ERR_DUPLICATE_FIELD`, `ERR_EMPTY_NAME`, `ERR_BAD_NAME` |
| `error_field` | `char[16]` | имя проблемного ключа (`leaf_g_per_100ml`) или пусто |
| `file_name` | `char[32]`* | basename файла для текста диалога |

\* или `FuriString*`, если решено не ограничивать длину имени; ограничение предпочтительнее
по RAM (см. plan.md, G6).

Соответствие сообщений (английский, короткие, FR-020/FR-021):

| `result` | Текст диалога |
|---|---|
| `ERR_TOO_LARGE` | `File is larger than 1 KB` |
| `ERR_NO_FIELD` | `Missing field: %s` |
| `ERR_BAD_NUMBER` | `Not a number: %s` |
| `ERR_OUT_OF_RANGE` | `Out of range: %s` |
| `ERR_DUPLICATE_FIELD` | `Duplicate field: %s` |
| `ERR_EMPTY_NAME` | `Name is empty` |
| `ERR_BAD_NAME` | `Bad name: %s` (`error_field` = `name`) |
| `ERR_NOT_FOUND` | `Cannot open file` |

---

## 6. Связи сущностей

```text
TeaPreset (1) ──── хранится в ────► BrewSession (0..1 активна)
      │                                   │
      │ source_path                       │ проецируется в
      ▼                                   ▼
 .tea файл на SD                     BrewViewModel (0..1 на экране)
                                          │
                                          ▼
                                    brew_view (Canvas)
```

- Один пресет — одна сессия; выбор другого пресета уничтожает текущую сессию
  (Assumptions «Один пресет — одна сессия»).
- `BrewViewModel` — производная, не источник истины; при уничтожении view сессия цела.

---

## 7. Валидация: сводная таблица

Все проверки логики выполняются **юнит-тестами на устройстве** (принцип VIII конституции
3.0.1): наборы `tea_preset` и `brew_math` в `unit_tests/`, запуск командой
`tea_timer_test` через `ufbt cli`. Имя набора указано в колонке «Тест».

| Правило | Проверяется в | Тест (набор на устройстве) |
|---|---|---|
| 5 обязательных полей, каждое ровно один раз | `tea_preset_parse_buf()` | `tea_preset` |
| `leaf_g_per_100ml` 0.1–50, одна точка | парсер + `number_input` (1…500 десятых) | `tea_preset` + ручной сценарий |
| `first_pour_sec` 1–3600 | парсер + `number_input` | `tea_preset` |
| `increment_sec` 0–600 | парсер + `number_input` | `tea_preset` |
| `max_pours` 1–99 | парсер + `number_input` | `tea_preset` |
| размер файла ≤ 1 КБ | `tea_preset_load()` до `read` | `tea_preset` (граница 1024/1025) |
| BOM, CRLF, `#`, пустые строки, пробелы вокруг `:` | `tea_preset_parse_buf()` | `tea_preset` |
| мусор после числа (`20abc`) | `tea_preset_parse_buf()` (`endptr`) | `tea_preset` |
| пустой `name`, `name` > 24 символов | `tea_preset_parse_buf()` | `tea_preset` |
| чтение реального файла и ошибка открытия | `tea_preset_load()` | `tea_preset` (временный каталог `APP_DATA_PATH("test")`) |
| инкремент только в `RUNNING → DONE` | `brew_session_complete()` | `brew_math` |
| отсутствие инкремента при прерывании | `brew_session_abort()` | `brew_math` |
| `next_pour_sec = prev + increment` | `brew_math_pour_sec()` | `brew_math` |
| формат `mm:ss` / `hh:mm:ss` (до 63000 с) | `brew_math_format_time()` | `brew_math` |
| признак превышения `completed > max` | `brew_session_over_limit()` | `brew_math` |
