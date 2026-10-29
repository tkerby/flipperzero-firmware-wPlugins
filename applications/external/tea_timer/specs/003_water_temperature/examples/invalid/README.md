# Заведомо некорректные примеры `.tea`

Каждый файл должен отклоняться приложением с диалогом `Cannot use file`; карточка не
открывается, приложение не падает. Точные коды и имена ключей —
[../../contracts/tea-file-format.md](../../contracts/tea-file-format.md).

| Файл | Нарушение | Ожидаемое сообщение |
|---|---|---|
| `missing-max.tea` | нет `water_temp_max_c` | `Missing field: water_temp_max_c` |
| `missing-min.tea` | нет `water_temp_min_c` | `Missing field: water_temp_min_c` |
| `low-84.tea` | минимум 84 | `Out of range: water_temp_min_c` |
| `high-91.tea` | максимум 91 | `Out of range: water_temp_max_c` |
| `inverted-88-86.tea` | минимум больше максимума | `Out of range: water_temp_max_c` |
| `duplicate-min.tea` | ключ минимума повторён | `Duplicate field: water_temp_min_c` |
| `fractional.tea` | дробное значение | `Not a number: water_temp_min_c` |
| `garbage.tea` | мусор после числа | `Not a number: water_temp_min_c` |
