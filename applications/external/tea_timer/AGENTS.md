# AGENTS.md

## О проекте

Это приложение (FAP) для **Flipper Zero** — портативного устройства с микроконтроллером
STM32WB55 (ARM Cortex-M4, 256 КБ RAM, 1 МБ Flash), экраном 128×64, ИК-портом, NFC/RFID,
Sub-1 GHz, iButton, GPIO и Bluetooth.

Проект — внешнее приложение (`FlipperAppType.EXTERNAL`), которое устанавливается на
устройство как отдельный `.fap`-файл и не требует пересборки прошивки.

Типовой состав проекта:

```
tea_timer/
├── application.fam        # манифест приложения (appid, точка входа, зависимости, стек)
├── *.c / *.h              # исходный код приложения
├── icons/                 # иконки (10px.png для меню)
├── images/                # ассеты для fap_icon_assets
├── .specify/              # Spec Kit: шаблоны, конституция, скрипты
└── .opencode/commands/    # слэш-команды Spec Kit (/speckit.*)
```

Манифест `application.fam` обязателен: в нём задаются `appid`, `entry_point`,
`requires` (сервисы прошивки: `gui`, `storage`, `notification`, `bt`, ...),
`stack_size`, `fap_version`, `fap_category`, `fap_icon`, `fap_icon_assets`.

## Сборка: только `ufbt`

Проект собирается **исключительно** утилитой `ufbt` (uFBT — micro Flipper Build Tool).
Она сама скачивает нужный SDK и toolchain в `~/.ufbt` и не зависит от дерева прошивки.

**Не использовать** систему сборки из каталога прошивки (`./fbt`, `scons`, `firmware.scons`):
этот репозиторий (`flipperzero-firmware`) — лишь рабочая папка, в которой лежит приложение,
и его сборка к данному проекту отношения не имеет.

Все команды выполняются из каталога приложения:

```bash
cd applications_user/tea_timer
ufbt                 # собрать приложение (текущий каталог)
```

Полезные варианты сборки:

```bash
ufbt APPID=tea_timer  # явно указать appid (если в каталоге несколько манифестов)
ufbt faps             # собрать все FAP-приложения, найденные в каталоге
ufbt FORCE=1          # принудительно пересобрать
ufbt VERBOSE=1        # показывать полные команды компиляции/линковки
ufbt UFBT_APP_DIR=/путь/к/приложению   # собрать приложение из другого каталога
```

Результат сборки: `dist/tea_timer.fap` (имя = `appid` из `application.fam`).

## Загрузка и запуск на Flipper

```bash
ufbt launch          # собрать, загрузить по USB и сразу запустить на устройстве
```

Устройство должно быть подключено по USB. Если подключено несколько Flipper:

```bash
ufbt launch FLIP_PORT=/dev/cu.usbmodemflip_XXXX   # macOS
```

## Просмотр логов

```bash
ufbt cli             # открыть CLI-сессию Flipper по USB (CDC)
```

Внутри CLI:

```
log                  # начать вывод логов с текущим уровнем
log debug            # подробный уровень (влияет на производительность)
log trace            # максимально подробный
log warn | log error | log info | log default
log ?                # справка по уровням
```

Остановка вывода логов — `Ctrl+C`, выход из CLI — `Ctrl+C` (или закрыть сессию).
Если нужен «сырой» последовательный порт, CLI доступен и через любой терминал
(115200 8N1) на CDC-порту устройства.

## Другие полезные команды

### Разработка

```bash
ufbt create APPID=myapp     # создать шаблон нового приложения в текущем каталоге
ufbt vscode_dist            # подготовить каталог для разработки в VSCode/CLion
ufbt cdb                    # сгенерировать compile_commands.json для clangd/CLion
ufbt format                 # отформатировать C-код (clang-format)
ufbt lint                   # статический анализ C-кода
ufbt update                 # обновить SDK/toolchain
ufbt update -c dev          # переключиться на канал dev (также rc / release)
ufbt update -t flipperZero  # явно указать аппаратную цель
eval $(ufbt -s env)         # войти в окружение toolchain (arm-none-eabi-gcc и т.п.)
```

### Прошивка и отладка

```bash
ufbt flash_usb        # обновить прошивку по USB (из self-update пакета)
ufbt flash_usb_full   # полная переустановка прошивки по USB
ufbt flash            # прошить через SWD-программатор (ST-Link/CMSIS-DAP/Black Magic)
ufbt debug            # запустить GDB-сессию
```

Параметры SWD-отладки: `SWD_TRANSPORT`, `SWD_TRANSPORT_SERIAL`, `OPENOCD_OPTS`,
`OTHER_ELF` (отладка заранее собранного ELF).

### Диагностика на устройстве (через `ufbt cli`)

```
help / ?        # список команд
info            # сведения об устройстве и прошивке
device_info     # подробная информация
free            # свободная память кучи
free_blocks     # карта свободных блоков кучи
top [ms]        # потоки, стек, CPU, heap (аналог top)
sysctl          # параметры системы
uptime          # время работы
date            # дата/время
gpio / i2c      # работа с GPIO и сканирование I2C
```

## Правила для агента

- Собирать, запускать и отлаживать — только через `ufbt`; не вызывать `./fbt` из корня репозитория.
- Рабочий каталог для команд сборки — каталог приложения (`applications_user/tea_timer`).
- Не редактировать файлы прошивки в `flipperzero-firmware/` — изменения только в этом каталоге.
- API прошивки: `furi.h`, `furi_hal.h`, `gui/*`, `storage/*`, `notification/*`;
  доступные сервисы перечисляются в `requires` манифеста.
- Память ограничена (256 КБ RAM, общий стек FAP обычно 1–2 КБ на поток) — следить за
  `stack_size` и размерами буферов; проверять `free`/`top` в CLI.
- Все выходные данные, спеки и документация — **на русском языке**.
