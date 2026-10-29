# Контракт формата `.tea` (версия 003 — диапазон температуры воды)

Формат — часть внешнего контракта приложения: файлы создаются вручную на компьютере
(001) и на самом устройстве (002), поэтому изменение только аддитивное и не ломает
ранее созданные файлы. Базовая версия — `001-tea-brew-timer/contracts/tea-file-format.md`;
этот документ описывает **расширение 003** и полностью заменяет таблицу обязательных
полей для новых файлов.

---

## 1. Файл

| Свойство | Значение |
|---|---|
| Расширение | `.tea` |
| Кодировка | ASCII/UTF-8; запись — без BOM; чтение допускает BOM `EF BB BF` |
| Перевод строки | запись — `LF`; чтение допускает `CRLF` |
| Максимальный размер | **1024 байта**; больше — `ERR_TOO_LARGE` |
| Структура | построчно `ключ: значение`, одна пара на строку |
| Комментарии | строка, первый непробельный символ которой `#`, игнорируется |
| Пустые строки | игнорируются |
| Неизвестные ключи | игнорируются (совместимое расширение) |
| Дубликат известного ключа | ошибка `ERR_DUPLICATE_FIELD` |

---

## 2. Обязательные поля (новый формат)

| Ключ | Тип | Допустимый диапазон | Пример | Ошибки |
|---|---|---|---|---|
| `name` | строка | 1–24 символа; латиница, цифры, пробел, `_` | `Green Tea` | `ERR_EMPTY_NAME`, `ERR_BAD_NAME` |
| `leaf_g_per_100ml` | десятичное, точка, ровно одна цифра после неё | `0.1`–`50.0` | `5.0` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |
| **`water_temp_min_c`** | целое | **85–90** | `85` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |
| **`water_temp_max_c`** | целое | **85–90** | `90` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |
| `first_pour_sec` | целое | `1`–`3600` | `20` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |
| `increment_sec` | целое | `0`–`600` | `5` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |
| `max_pours` | целое | `1`–`99` | `6` | `ERR_OUT_OF_RANGE`, `ERR_BAD_NUMBER` |

Дополнительно для пары температуры:

- `water_temp_min_c <= water_temp_max_c`; равенство допустимо (`87`/`87`);
- нарушение порядка — `ERR_OUT_OF_RANGE` с `error_field = "water_temp_max_c"`.

---

## 3. Совместимость (FR-005, FR-006)

| Содержимое файла | Результат | Значение в модели |
|---|---|---|
| Оба ключа температуры отсутствуют | `OK` | `85` / `90` (умолчание) |
| Оба ключа присутствуют и корректны | `OK` | значения из файла |
| Присутствует только `water_temp_min_c` | `ERR_NO_FIELD` | файл отклоняется; `error_field = "water_temp_max_c"` |
| Присутствует только `water_temp_max_c` | `ERR_NO_FIELD` | файл отклоняется; `error_field = "water_temp_min_c"` |
| Оба ключа есть, `min > max` | `ERR_OUT_OF_RANGE` | файл отклоняется; `error_field = "water_temp_max_c"` |

Правило «умолчание только при отсутствии обоих ключей» — единственное допустимое:
оно сохраняет работоспособность всех файлов 001/002 и одновременно не маскирует
недописанную пару.

---

## 4. Эталонный файл (новый формат)

```tea
# Green Tea - 5 g per 100 ml, 6 pours, water 85-90 C
name: Green Tea
leaf_g_per_100ml: 5.0
water_temp_min_c: 85
water_temp_max_c: 90
first_pour_sec: 20
increment_sec: 5
max_pours: 6
```

Ожидаемая модель: `leaf_tenths_g = 50`, `water_temp_min_c = 85`,
`water_temp_max_c = 90`, `first_pour_sec = 20`, `increment_sec = 5`, `max_pours = 6`.

---

## 5. Пример старого файла (001/002) — тоже корректен

```tea
# Green Tea - 5 g per 100 ml, 6 pours
name: Green Tea
leaf_g_per_100ml: 5.0
first_pour_sec: 20
increment_sec: 5
max_pours: 6
```

Ожидаемая модель: те же значения, `water_temp_min_c = 85`, `water_temp_max_c = 90`.
При правке на устройстве оба ключа добавляются в файл, остальные значения и имя файла
сохраняются.

---

## 6. Порядок записи

Новые и отредактированные файлы пишутся в порядке:

```text
name
leaf_g_per_100ml
water_temp_min_c
water_temp_max_c
first_pour_sec
increment_sec
max_pours
```

Правила:

- ровно одна пара на строку, `ключ: значение`, `LF`, завершающий `\n`;
- `leaf_g_per_100ml` — `%u.%u` (ровно одна цифра после точки);
- оба ключа температуры присутствуют **всегда** (FR-004);
- кодировка ASCII, комментарии не пишутся;
- после записи файл читается обратно тем же парсером; сравнение включает оба значения
  температуры, иначе — `PresetSaveErrVerify`.

---

## 7. Алгоритм разбора (дополнение к 001)

1. Шаги 1–10 из контракта 001 без изменений (BOM, строки, `#`, первое `:`, обрезка).
2. Перед разбором `out` инициализируется умолчаниями `water_temp_min_c = 85`,
   `water_temp_max_c = 90`.
3. Для ключей `water_temp_min_c` / `water_temp_max_c`:
   - дубликат → `ERR_DUPLICATE_FIELD`, `error_field` = имя ключа;
   - значение разбирается как неотрицательное целое (только цифры, весь фрагмент
     израсходован); иначе → `ERR_BAD_NUMBER`;
   - значение вне 85–90 → `ERR_OUT_OF_RANGE`.
4. После цикла:
   - найден только один ключ → `ERR_NO_FIELD` с именем отсутствующего;
   - найдены оба → проверить `min <= max`, иначе `ERR_OUT_OF_RANGE` с
     `error_field = "water_temp_max_c"`;
   - не найден ни один → оставить умолчания `85`/`90`.
5. Остальные обязательные поля проверяются как в 001.

---

## 8. Тестовые векторы (набор `tea_preset`, на устройстве)

| Вход | Ожидание |
|---|---|
| эталонный файл нового формата | `OK`, `85/90` |
| `water_temp_min_c: 85`, `water_temp_max_c: 87` | `OK`, `85/87` |
| `water_temp_min_c: 87`, `water_temp_max_c: 87` | `OK`, `87/87` |
| оба ключа отсутствуют (файл 001) | `OK`, `85/90` |
| `water_temp_min_c: 84` | `ERR_OUT_OF_RANGE`, `water_temp_min_c` |
| `water_temp_max_c: 91` | `ERR_OUT_OF_RANGE`, `water_temp_max_c` |
| `water_temp_min_c: 88`, `water_temp_max_c: 86` | `ERR_OUT_OF_RANGE`, `water_temp_max_c` |
| только `water_temp_min_c` | `ERR_NO_FIELD`, `water_temp_max_c` |
| только `water_temp_max_c` | `ERR_NO_FIELD`, `water_temp_min_c` |
| `water_temp_min_c` дважды | `ERR_DUPLICATE_FIELD`, `water_temp_min_c` |
| `water_temp_max_c` дважды | `ERR_DUPLICATE_FIELD`, `water_temp_max_c` |
| `water_temp_min_c: 85.5` | `ERR_BAD_NUMBER` |
| `water_temp_min_c: 85abc` | `ERR_BAD_NUMBER` |
| `water_temp_min_c:  085 ` | `OK`, `85` |
| `water_temp: 85-90` (неизвестный ключ) | `OK`, поле игнорируется |

Для кривых входов используется буферный слой `tea_preset_parse_buf()`; файловые
сценарии — во временном каталоге `APP_DATA_PATH("test")`.
