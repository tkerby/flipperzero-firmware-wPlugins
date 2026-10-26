# Devboard notes

Hard-won facts about the official Flipper WiFi devboard (ESP32-S2-WROVER).
Everything here cost real debugging time at least once.

## LEDs

Three discrete LEDs, **not** a NeoPixel, and **active low** — driving the pin
LOW lights it:

| Colour | GPIO |
|--------|------|
| Blue   | 4    |
| Green  | 5    |
| Red    | 6    |

Source: FlipperHTTP's `BOARD_WIFI_DEV` branch in `src/flipper-http/led.cpp`.

`status_led.c` drives these from the first line of `app_main()`. That is
deliberate and worth keeping: **a board showing nothing is indistinguishable
from a board that is broken.** An afternoon was lost to a perfectly healthy
board that simply had no LED code, because our firmware — unlike FlipperHTTP —
never touched the pins.

## Console lives on USB, not UART0

`CONFIG_ESP_CONSOLE_USB_CDC=y`, which sets `CONFIG_ESP_CONSOLE_UART_NUM=-1`.

UART0 is the only port the board brings out to the Flipper's header, so the
link owns it outright. Two consequences, both good:

- No log line can ever land inside a frame and corrupt it.
- The board enumerates on a computer as `VID:PID=303A:0002`, so it can be
  monitored like any other dev board.

Logging never blocks when no USB host is attached: `cdc_acm_fifo_fill()`
returns 0 and `esp_usb_console_flush_internal()` rolls the write back. Verified
in the IDF source before enabling it, because the board normally runs headless
on the Flipper.

Two USB identities, and the difference matters when debugging:

| Serial number      | Meaning                      |
|--------------------|------------------------------|
| `c0:4e:30:60:0f:94` (the MAC) | firmware running  |
| `0`                | ROM bootloader, download mode |

## Flashing

**Over USB (preferred — ~5 s):** the board has no auto-reset circuit, so
esptool cannot put it into download mode by itself. Hold **BOOT**, tap
**RESET**, release **BOOT**. The ROM reboots out of download mode on its own if
no host starts talking, so start esptool *first* and press the buttons while it
retries (`tools/flash_usb.sh`). `--after no_reset` leaves it in the bootloader:
unplug and replug to boot normally. A plain RESET press may not be enough.

**Over the Flipper (ESP Flasher, ~86 s):** slots default to exactly our
offsets — boot `0x1000`, partition `0x8000`, app A `0x10000`. Leave S3/C5 mode
off; those move the bootloader to `0x0`/`0x2000`.

- Use **Flash slow**, never **Flash fast**. Turbospeed calls
  `esp_loader_change_transmission_rate(921600)` mid-handshake; that is the best
  explanation for a flash that reports failure after having already erased.
- **Never use "Enter Bootloader".** It calls `furi_hal_power_disable_otg()` /
  `enable_otg()`, which is meant for third-party boards and crashed the Flipper.
- **This route cannot put the board into download mode by itself.** The official
  devboard does not wire IO0/EN to the Flipper header, so ESP Flasher reports
  "board is not in reflash mode" unless the chip is *already* in the bootloader.
  The one time it worked, a previous USB flash had used `--after no_reset` and
  left it there. Treat ESP Flasher as a follow-on to a USB flash, not an
  independent path.

## Diagnosing

The USB console is the whole debugging story. A boot loop that looked like dead
hardware turned out to be, in one capture:

```
I (240) main_task: Calling app_main()
assert failed: xQueueSemaphoreTake queue.c:1709 (( pxQueue ))
```

— the LED task taking `hub.lock` before `stream_hub_init()` created it. Hence
`stream_hub_ready()`: anything started before the hub must ask first.

The Flipper's own screen is readable over RPC when the board is on the header
(`gui_start_screen_stream_request`, field 20 → `ScreenFrame`, field 22; 128x64,
1bpp, bit `y%8` of byte `(y/8)*128 + x`). Send `start_rpc_session\r` with a bare
CR — a trailing `\n` is left in the buffer and desyncs every length prefix after
it, which shows up as `ERROR_DECODE`.

## Flashing without the BOOT/RESET dance

The devboard has no auto-reset circuit on its USB-C port, so the usual advice is
"hold BOOT, tap RESET, release BOOT". That is only necessary when the chip is
not already running our firmware.

Because `CONFIG_ESP_CONSOLE_USB_CDC=y` puts the console on the S2's native USB
peripheral, esptool can drive the USB-OTG reset over that same connection:

    python3 -m esptool --chip esp32s2 -p /dev/cu.usbmodemXXXX \
        --before usb_reset --after hard_reset write_flash ...

No buttons, ~5 s, works while the firmware is running. `tools/flash_usb.sh`
tries this first and only falls back to the button loop if it fails.

Telling the two USB identities apart matters here:

| `SER=` in the hwid       | what it means                  |
|--------------------------|--------------------------------|
| `c0:4e:30:60:0f:94` (MAC)| our firmware is running        |
| `0`                      | ROM bootloader / download mode |

A board that shows the MAC serial after a BOOT+RESET attempt did **not** enter
download mode -- it just rebooted into the app.

## Verifying a flash

"Hash of data verified" means the bytes are on the chip. It says nothing about
whether they run -- a firmware that aborts in `app_main` verifies perfectly and
then boot-loops. Always confirm the chip actually boots:

1. **Power-cycle it.** `--after hard_reset` drives RTS, which this board does
   not wire to EN, so after a flash it stays in the ROM bootloader until it
   loses power. Unplug and replug.
2. **Re-resolve the port.** It is renamed by the identity change, so a command
   aimed at the old name fails silently:

   | State            | Serial        | macOS port name       |
   |------------------|---------------|-----------------------|
   | app running      | the chip MAC  | `cu.usbmodem1101`     |
   | ROM download mode| `0`           | `cu.usbmodem01`       |

3. **Watch the port for ~12 s.** A boot loop re-enumerates USB over and over,
   so repeated appear/disappear transitions are the tell. Stable with the MAC
   serial and a quiet console means it is up; the broken build printed its boot
   banner continuously.

A crash-looping app also breaks `--before usb_reset`, because that works by
talking to the USB console *the running firmware provides*. Flashing a firmware
that aborts during init takes away the mechanism needed to replace it, and the
only ways back are the BOOT button or catching the short window each boot cycle
by retrying.

## ESP-IDF gotchas that cost real time

- `httpd_config_t.max_uri_handlers` defaults to **8**. Registering a ninth route
  returns `ESP_ERR_HTTPD_HANDLERS_FULL`, and `ESP_ERROR_CHECK` turns that into
  `abort()` -- a boot loop from nothing but adding a URL. `http_srv.c` derives
  the limit from the route table so it cannot drift again.

## Reading the board's log live (and the trap in it)

The console is on USB CDC, so the board's own `ESP_LOG` output is the best
diagnostic tool available -- but two things have to be right or it looks like
the board is silent when it is not.

1. **Something must assert DTR.** The S2's CDC driver throws output away when no
   host has the port open (`esp_usb_console_write_buf` -> `cdc_acm_fifo_fill`
   returns 0 and the write is rolled back). `cat /dev/cu.usbmodem1101` opens the
   port *without* asserting DTR -- which is exactly why `cu` is safe to use on a
   board that must not be reset -- so it captures **nothing**. Open it with
   pyserial instead -- but **set DTR after `open()`, never before.** pyserial
   applies the flags *during* `open()`, and that transition looks enough like
   esptool's reset sequence to halt the chip and stall the CDC endpoint. The
   order that works is: `dtr = False`, `rts = False`, `open()`, settle ~0.3 s,
   then `dtr = True` -- one clean transition, after the port is already up.
   Getting this backwards cost two days (see "The USB wedge that survives
   everything" below).

2. **Never kill the reader with `SIGKILL`.** Dying with DTR asserted leaves the
   CDC endpoint stalled with a full TX queue and no reader. The board keeps
   running, but *every* `/dev` node for it then hangs in `os.open()` -- `cu.*`
   and `tty.*` alike, with no process holding either -- so logging and flashing
   are both blocked until the cable is physically replugged. Close the port and
   drop DTR on the way out, trapping SIGTERM/SIGINT/SIGHUP to do it.

Starting a monitor immediately after `write_flash` hits the same wedge: the chip
is still re-enumerating, so the monitor grabs the dying device node. Wait for
the port to disappear and come back before attaching.

Because the write path never blocks, log lines are free when no cable is
attached -- a once-a-second heartbeat costs nothing on the Flipper.

## The USB wedge that survives everything

A stalled CDC endpoint is **not** cleared by a reset, a reboot, or a reflash,
and this is by design:

    config ESP32S2_KEEP_USB_ALIVE
        "Keep USB peripheral enabled at start up"
        default y if ESP_CONSOLE_USB_CDC
        help: During application initialization, all the peripherals except
              UARTs and timers are reset. Enable this option to keep USB enabled.

`esp_system/port/soc/esp32s2/clk.c` leaves `DPORT_USB_CLK_EN` out of the
peripheral reset mask when it is set, and it is forced on by
`CONFIG_ESP_CONSOLE_USB_CDC`. So USB is the one peripheral the firmware
deliberately does not reset -- which is what we want for a console that
survives reboots, and exactly wrong once the endpoint is stalled.

The symptom is a board that is **completely healthy and invisible at the same
time**: softAP up, captive portal serving, audio streaming to listeners, and no
`/dev/cu.*` node on any computer, with any cable. Those are not contradictory,
and chasing the firmware for the cause is wasted time -- the firmware is fine.

Only removing power clears it, and the board has **two** power sources. Pulling
USB while it sits on the Flipper header leaves it fully powered from the
Flipper, so the peripheral never loses state. Take it off the header **and** out
of USB together, leave it dead ~30 s, then plug USB in alone.

## The board was out of RAM (the bug behind almost everything)

Symptoms that all turned out to be one cause: audio playing for 1-2 seconds and
stopping, `stream error [code 3 net 1 ready 1]`, the phone reporting "no
internet connection", slow or failed Wi-Fi rejoins, and the captive popup
showing a black screen.

The heartbeat printed the answer:

    hb audio=0 frames=2 crc=0 resync=0 credit=65535 listeners=0 buf=0% \
       heap=27036 largest=9728

**27 KB of free internal heap at idle, largest contiguous block 9.7 KB**, with no
PSRAM enabled. At that level:

- `xTaskCreate` for a listener's 6 KB stack fails intermittently -- the
  `could not start stream task` errors, which is why the first couple of
  attempts to listen did nothing at all.
- The Wi-Fi driver and lwip cannot get transmit buffers, so packets are dropped
  and TCP backs off. Measured result: a listener received **18047 bytes in 12
  seconds (~1.5 KB/s)** when 128 kbps audio needs 16 KB/s. Nothing about the MP3
  path was wrong; the radio simply could not transmit.

The module is an ESP32-S2-**WROVER**, which carries 2 MB of external RAM the
build was ignoring. `CONFIG_SPIRAM=y` with `CONFIG_SPIRAM_IGNORE_NOTFOUND=y`
turns it on without risking a board that has none -- note that the safety flag
also disables `SPIRAM_TRY_ALLOCATE_WIFI_LWIP`, so Wi-Fi buffers stay internal
either way. The win is the internal RAM the 64 KB audio ring stops occupying.

Watch for `Found 2MB external RAM` in the boot log to confirm the module has it.

## Measure, do not infer

Three separate wrong turns in one night came from reasoning about this board
instead of reading it: claiming a flash was healthy from a stable USB port
(CDC survives soft resets, so it proves nothing), claiming the AP was down from
`system_profiler SPAirPortDataType` (it prints `<redacted>` for every SSID
without Location permission, so it can never find one), and chasing the MP3
decoder when the link counters showed `crc=0` and a frozen `resync`. The
heartbeat line costs one build and answers all three.
