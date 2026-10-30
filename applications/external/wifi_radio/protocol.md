# Flipper Radio — UART protocol

Link between the Flipper (FAP) and the ESP32-S2 devboard over the GPIO header.

- **Pins:** Flipper TX = pin 13, RX = pin 14 (USART). The FAP must call
  `expansion_disable()` before claiming the UART and `expansion_enable()` on exit,
  or the firmware's expansion-module listener owns the pins.
- **Baud:** 921600 (fallback 460800 if the link shows errors — still 3x the
  16 KB/s a 128 kbps stream needs).
- **Duplex:** half-duplex framed messages, both directions.

## Frame format

```
[0xAA][type:1][len:2 LE][payload:len][crc8]
```

- `0xAA` — start byte.
- `type` — message type (below).
- `len`  — payload length, little-endian u16 (payload ≤ 512 bytes).
- `crc8` — CRC-8 (poly 0x07) over `type | len | payload`.

## Flipper → ESP

| type | name        | payload                                   |
|------|-------------|-------------------------------------------|
| 0x01 | AUDIO_DATA  | raw MP3 bytes, ≤512 per frame             |
| 0x02 | TRACK_META  | JSON `{"title":"…","artist":"…"}`          |
| 0x03 | FLUSH       | (none) — track skip; ESP drops its buffer |
| 0x04 | SET_SSID    | UTF-8 station name (≤32 bytes)            |
| 0x05 | PING        | (none)                                    |

## ESP → Flipper

| type | name    | payload                                        |
|------|---------|------------------------------------------------|
| 0x81 | CREDIT  | u16 LE — bytes the ESP can still accept        |
| 0x82 | STATS   | JSON `{"listeners":N,"buf":0-100}`             |
| 0x83 | PONG    | (none)                                         |

## Flow control & pacing

- The Flipper paces `AUDIO_DATA` at the file's CBR rate (bytes/sec = bitrate/8),
  read from the first MP3 frame header. This is the clock.
- `CREDIT` is the overflow backstop: the Flipper never sends more than the
  granted window. The ESP grants credit as its ring buffer drains.
- ESP ring buffer target ~64 KB; browsers add a few seconds of their own buffer,
  which smooths over UART hiccups.

## Truncation & resync

A frame cut short mid-payload **cannot** be recovered by hunting for the next
`0xAA`: the start byte is not escaped, and `0xAA` occurs naturally inside MP3
data, so a mid-payload scan cannot tell a real frame boundary from audio.

Both halves therefore reset the decoder on an **idle gap**: if no byte arrives
for `RADIO_FRAME_IDLE_RESET_MS` (50 ms) while a frame is partially received,
abandon it and go back to hunting for a start byte. At 921600 baud a full
512-byte frame takes ~5.6 ms, so 50 ms is far longer than any legitimate
inter-byte gap.

Without this reset, one truncated frame also destroys the frame after it — the
stale decoder consumes the next frame's start byte as payload, and that frame
then fails CRC. Verified by the host tests (`radio_decoder_reset()` in
`radio_proto.c`).

Decoders keep counters — `frames_ok`, `crc_errors`, `oversize_errors`,
`resyncs`, `truncated` — so a flaky link is visible on the station console
rather than just sounding bad.

## Notes

- MP3 only, constant bitrate — browsers decode natively, neither chip touches audio.
- `TRACK_META` drives the "now playing" text on both the Flipper screen and the
  `/status` endpoint the player page polls.
