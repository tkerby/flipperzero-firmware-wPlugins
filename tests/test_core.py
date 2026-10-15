#!/usr/bin/env python3
"""
Tests for the PN532 and CCID/PC-SC engines, run on a PC against a simulated card.

    cd nfc_bridge && make test

The PN532 side is exercised the way libnfc's pn532_uart driver talks to a chip
(wake-up preamble, Diagnose echo test, SAMConfiguration, RFConfiguration,
InListPassiveTarget, InDataExchange for MIFARE, InCommunicateThru, ...).
The CCID side sends raw CCID bulk messages like pcscd's libccid does.
"""
import ctypes as C
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
lib = C.CDLL(os.path.join(HERE, "libnbtest.so"))
lib.t_pn_drain.restype = C.c_size_t
lib.t_ccid.restype = C.c_size_t
lib.t_atr.restype = C.c_size_t

NONE, MFC1K, UL, ISO4A = 0, 1, 2, 3
A_KEY = bytes.fromhex("A0A1A2A3A4A5")
B_KEY = bytes.fromhex("B0B1B2B3B4B5")


def reset(card):
    lib.t_reset(card)


def buf(b):
    return (C.c_ubyte * max(len(b), 1)).from_buffer_copy(bytes(b) if b else b"\0")


# ----------------------------------------------------------------- PN532 side
def frame(payload):
    """Host -> PN532 frame around TFI D4 + payload."""
    body = bytes([0xD4]) + bytes(payload)
    n = len(body)
    if n < 256:
        head = bytes([0x00, 0x00, 0xFF, n, (0x100 - n) & 0xFF])
    else:
        lm, ll = n >> 8, n & 0xFF
        head = bytes([0x00, 0x00, 0xFF, 0xFF, 0xFF, lm, ll, (0x100 - (lm + ll)) & 0xFF])
    return head + body + bytes([(0x100 - sum(body)) & 0xFF, 0x00])


def pn_raw(data, chunk=None):
    data = bytes(data)
    if chunk:
        for i in range(0, len(data), chunk):
            lib.t_pn_feed(buf(data[i:i + chunk]), len(data[i:i + chunk]))
    else:
        lib.t_pn_feed(buf(data), len(data))
    out = (C.c_ubyte * 4096)()
    n = lib.t_pn_drain(out, 4096)
    return bytes(out[:n])


def split_frames(raw):
    """Split a PN532->host byte stream into ('ack'|'nack'|'err'|'resp', payload)."""
    i, res = 0, []
    while i < len(raw):
        assert raw[i:i + 3] == b"\x00\x00\xff", "bad start code at %d: %s" % (i, raw[i:i + 8].hex())
        ln, lcs = raw[i + 3], raw[i + 4]
        if (ln, lcs) == (0x00, 0xFF):
            res.append(("ack", b""))
            i += 6
            continue
        if (ln, lcs) == (0xFF, 0x00):
            res.append(("nack", b""))
            i += 6
            continue
        if (ln, lcs) == (0xFF, 0xFF):
            lm, ll, lcs2 = raw[i + 5], raw[i + 6], raw[i + 7]
            assert (lm + ll + lcs2) & 0xFF == 0
            n, hdr = (lm << 8) | ll, 8
        else:
            assert (ln + lcs) & 0xFF == 0
            n, hdr = ln, 5
        body = raw[i + hdr:i + hdr + n]
        dcs = raw[i + hdr + n]
        assert (sum(body) + dcs) & 0xFF == 0, "bad DCS"
        assert raw[i + hdr + n + 1] == 0x00
        i += hdr + n + 2
        if body == b"\x7f":
            res.append(("err", b""))
        else:
            assert body[0] == 0xD5
            res.append(("resp", body[1:]))
    return res


def pn(cmd_and_data, chunk=None):
    """Send one command, return the response payload (starting with cmd+1)."""
    fr = split_frames(pn_raw(frame(cmd_and_data), chunk))
    assert fr[0][0] == "ack", fr
    assert len(fr) == 2, fr
    assert fr[1][0] == "resp", fr
    assert fr[1][1][0] == cmd_and_data[0] + 1
    return fr[1][1][1:]


def list_target():
    return pn([0x4A, 0x01, 0x00])


def dex(*parts):
    return pn([0x40, 0x01] + [b for p in parts for b in bytes(p)])


# ------------------------------------------------------------------- tests ---
def test_pn532_startup_like_libnfc():
    reset(MFC1K)
    # wake-up preamble + SAMConfiguration(normal mode), exactly what libnfc sends first
    wake = bytes([0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    out = pn_raw(wake + frame([0x14, 0x01, 0x00, 0x00]))
    fr = split_frames(out)
    assert fr == [("ack", b""), ("resp", bytes([0x15]))], fr
    # communication-line test used by pn53x_check_communication
    assert pn([0x00, 0x00] + list(b"libnfc")) == bytes([0x00]) + b"libnfc"
    assert pn([0x02]) == bytes([0x32, 0x01, 0x06, 0x07])
    assert pn([0x12, 0x14]) == b""  # SetParameters
    assert pn([0x32, 0x05, 0xFF, 0x01, 0x02]) == b""  # RFConfiguration (retries)
    assert pn([0x32, 0x01, 0x01]) == b""  # RF field on
    assert pn([0x16, 0xFF]) == bytes([0x00])  # PowerDown acknowledged


def test_pn532_registers():
    reset(MFC1K)
    assert pn([0x06, 0x63, 0x02, 0x63, 0x03]) == bytes([0x80, 0x80])  # defaults: CRC on
    assert pn([0x08, 0x63, 0x02, 0x00, 0x63, 0x03, 0x00]) == b""
    assert pn([0x06, 0x63, 0x02, 0x63, 0x03, 0x12, 0x34]) == bytes([0x00, 0x00, 0x00])


def test_pn532_list_targets():
    reset(MFC1K)
    # ATQA comes back MSB first (card sends 04 00, PN532 reports 00 04)
    assert list_target() == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04, 0xDE, 0xAD, 0xBE, 0xEF])
    # selecting a specific UID
    assert pn([0x4A, 0x01, 0x00, 0xDE, 0xAD, 0xBE, 0xEF])[0] == 1
    assert pn([0x4A, 0x01, 0x00, 0x11, 0x22, 0x33, 0x44]) == bytes([0x00])
    # unsupported modulation (FeliCa 212k): nothing found
    assert pn([0x4A, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00]) == bytes([0x00])
    lib.t_set_card(UL)
    assert list_target() == bytes([0x01, 0x01, 0x00, 0x44, 0x00, 0x07, 0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66])
    lib.t_set_card(ISO4A)
    ats = bytes.fromhex("0C7877B10280" "31C073C84000")
    assert list_target() == bytes([0x01, 0x01, 0x03, 0x04, 0x20, 0x04, 0x08, 0xAA, 0xBB, 0xCC]) + ats
    lib.t_set_card(NONE)
    assert list_target() == bytes([0x00])


def test_pn532_mifare_classic():
    reset(MFC1K)
    list_target()
    # reading before authenticating is refused (0x14 = authentication error)
    assert dex([0x30, 0x03]) == bytes([0x14])
    # wrong key
    assert dex([0x60, 0x03], b"\xff" * 6, bytes.fromhex("DEADBEEF")) == bytes([0x14])
    # right key A
    assert dex([0x60, 0x03], A_KEY, bytes.fromhex("DEADBEEF")) == bytes([0x00])
    trailer = A_KEY + bytes([0xFF, 0x07, 0x80, 0x69]) + B_KEY
    assert dex([0x30, 0x03]) == bytes([0x00]) + trailer
    assert dex([0x30, 0x01]) == bytes([0x00]) + bytes(range(16, 32))
    # a block in another sector needs its own authentication
    assert dex([0x30, 0x04]) == bytes([0x14])
    # write and read back
    new = bytes(range(0xA0, 0xB0))
    assert dex([0xA0, 0x01], new) == bytes([0x00])
    mem = (C.c_ubyte * 16)()
    lib.t_mfc_block(1, mem)
    assert bytes(mem) == new
    assert dex([0x30, 0x01]) == bytes([0x00]) + new
    # block 0 is read-only on this card: the failure is reported, not hidden
    assert dex([0xA0, 0x00], bytes(16)) != bytes([0x00])
    # key B authentication
    assert dex([0x61, 0x07], B_KEY, bytes.fromhex("DEADBEEF")) == bytes([0x00])
    assert dex([0x30, 0x05])[0] == 0x00
    # releasing the card ends the session
    assert pn([0x52, 0x00]) == bytes([0x00])
    assert dex([0x30, 0x05]) == bytes([0x27])
    assert lib.t_stat(1) >= 1


def test_pn532_ultralight():
    reset(UL)
    list_target()
    assert dex([0x30, 0x04]) == bytes([0x00]) + bytes(range(0x50, 0x60))
    assert dex([0xA2, 0x05], bytes.fromhex("DEADBEEF")) == bytes([0x00])
    pg = (C.c_ubyte * 4)()
    lib.t_ul_page(5, pg)
    assert bytes(pg) == bytes.fromhex("DEADBEEF")
    assert dex([0xA2, 0x02], bytes(4)) != bytes([0x00])  # locked page


def test_pn532_iso14443_4_is_stateful():
    reset(ISO4A)
    list_target()
    assert dex([0x00, 0xB0, 0x00, 0x00]) == bytes([0x00, 0x69, 0x85])  # not selected yet
    assert dex([0x00, 0xA4, 0x04, 0x00, 0x06], bytes.fromhex("F00102030405")) == bytes([0x00, 0x90, 0x00])
    assert dex([0x00, 0xB0, 0x00, 0x00]) == bytes([0x00]) + b"HELO" + bytes([0x90, 0x00])
    assert dex([0x00, 0xA4, 0x04, 0x00, 0x06], bytes.fromhex("112233445566")) == bytes([0x00, 0x6A, 0x82])


def test_pn532_extended_frames():
    reset(ISO4A)
    list_target()
    # answer longer than 255 bytes -> the engine must answer with an extended frame
    raw = pn_raw(frame([0x40, 0x01, 0x00, 0xC0, 0x00, 0x00]))
    assert raw[6:11] == b"\x00\x00\xff\xff\xff", raw[:12].hex()  # after the ACK: extended-frame header
    fr = split_frames(raw)
    assert fr[0][0] == "ack" and fr[1][0] == "resp"
    data = fr[1][1]
    assert data[0] == 0x41 and data[1] == 0x00
    assert data[2:260] == bytes(i & 0xFF for i in range(258)) and data[260:262] == b"\x90\x00"
    # a long command arriving as an extended frame
    long_apdu = bytes([0x00, 0xD6, 0x00, 0x00, 0xFF]) + bytes(255)
    fr = split_frames(pn_raw(frame([0x40, 0x01] + list(long_apdu))))
    assert fr[1] == ("resp", bytes([0x41, 0x00, 0x6D, 0x00]))


def test_pn532_comm_thru_bit_framing_and_crc():
    reset(MFC1K)
    list_target()
    pn([0x08, 0x63, 0x02, 0x00, 0x63, 0x03, 0x00, 0x63, 0x3D, 0x07])  # no CRC, 7-bit first byte
    assert pn([0x42, 0x26]) == bytes([0x00, 0xD9])
    assert lib.t_stat(4) == 7 and lib.t_stat(5) == 0 and lib.t_stat(6) == 0
    pn([0x08, 0x63, 0x02, 0x80, 0x63, 0x03, 0x80, 0x63, 0x3D, 0x00])  # CRC on, full bytes
    pn([0x42, 0x30, 0x04])
    assert lib.t_stat(4) == 16 and lib.t_stat(5) == 1 and lib.t_stat(6) == 1


def test_pn532_framing_robustness():
    reset(MFC1K)
    # one byte at a time
    assert split_frames(pn_raw(frame([0x02]), chunk=1)) == [("ack", b""), ("resp", bytes([0x03, 0x32, 0x01, 0x06, 0x07]))]
    # garbage in front
    fr = split_frames(pn_raw(bytes([0x12, 0x34, 0xAA]) + frame([0x02])))
    assert fr[-1] == ("resp", bytes([0x03, 0x32, 0x01, 0x06, 0x07]))
    # corrupted data checksum -> NACK, and the engine keeps working afterwards
    bad = bytearray(frame([0x02]))
    bad[-2] ^= 0xFF
    assert split_frames(pn_raw(bytes(bad))) == [("nack", b"")]
    assert split_frames(pn_raw(frame([0x02])))[-1][0] == "resp"
    # corrupted length checksum is skipped, next valid frame still works
    bad = bytearray(frame([0x02]))
    bad[4] ^= 0x55
    out = split_frames(pn_raw(bytes(bad) + frame([0x02])))
    assert out[-1] == ("resp", bytes([0x03, 0x32, 0x01, 0x06, 0x07]))
    # host NACK -> last response repeated
    first = pn_raw(frame([0x02]))
    again = pn_raw(bytes([0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00]))
    assert again == first[6:], (again.hex(), first.hex())
    # host ACK is swallowed silently
    assert pn_raw(bytes([0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00])) == b""


def test_pn532_unsupported_commands_get_error_frame():
    reset(MFC1K)
    fr = split_frames(pn_raw(frame([0x8C, 0x00])))  # TgInitAsTarget
    assert fr == [("ack", b""), ("err", b"")]
    # and the engine is still alive
    assert pn([0x02])[0] == 0x32


def test_ccid_basics():
    reset(MFC1K)
    r = ccid(0x62, b"", slot=1)  # only slot 0 exists
    assert len(r) == 10 and r[7] & 0x40 and r[8] == 5
    r = ccid(0x7F, b"")  # unknown message type
    assert r[0] == 0x81 and r[7] & 0x40 and r[8] == 0
    # truncated message / lying dwLength -> nothing sent
    out = (C.c_ubyte * 300)()
    assert lib.t_ccid(buf(b"\x62\x00"), 2, out, 300) == 0
    assert lib.t_ccid(buf(bytes([0x6F, 0xFF, 0x00, 0, 0, 0, 0, 0, 0, 0])), 10, out, 300) == 0


def test_ccid_atr_and_power():
    reset(MFC1K)
    r = ccid(0x65)  # GetSlotStatus: nothing reported yet -> absent
    assert r[0] == 0x81 and r[7] == 0x02
    r = ccid(0x62)
    assert r[0] == 0x80 and r[7] == 0x00  # DataBlock, ICC active, command ok
    atr = r[10:10 + (r[1] | r[2] << 8)]
    # the well-known PC/SC part 3 ATR for a MIFARE Classic 1K
    assert atr.hex() == "3b8f8001804f0ca0000003060300010000000" "06a", atr.hex()
    assert ccid(0x65)[7] == 0x00  # present and active
    r = ccid(0x63)  # power off
    assert r[0] == 0x81 and r[7] == 0x01  # present, inactive
    reset(NONE)
    r = ccid(0x62)
    assert r[7] == 0x42 and r[8] == 0xFE  # failed, no card, ICC_MUTE
    # ATRs for other card types are well formed (TCK = XOR of T0..last)
    for kind in (MFC1K, UL, ISO4A):
        out = (C.c_ubyte * 64)()
        n = lib.t_atr(kind, out, 64)
        a = bytes(out[:n])
        assert a[0] == 0x3B
        x = 0
        for b in a[1:]:
            x ^= b
        assert x == 0, "TCK wrong for %d: %s" % (kind, a.hex())
    out = (C.c_ubyte * 64)()
    n = lib.t_atr(ISO4A, out, 64)
    assert bytes(out[:n]).hex().startswith("3b87800180" "31c073c84000"), bytes(out[:n]).hex()
    n = lib.t_atr(UL, out, 64)
    assert bytes(out[:n])[12:16].hex() == "03000300", bytes(out[:n]).hex()  # SS=03, name=0003 (Ultralight)


def test_ccid_pcsc_pseudo_apdus_classic():
    reset(MFC1K)
    r = apdu_raw("FFCA000000")  # card not powered on yet
    assert r[7] & 0x40 and r[8] == 0xFE
    ccid(0x62)
    assert apdu("FFCA000000") == bytes.fromhex("DEADBEEF9000")
    assert apdu("FFCA000002") == bytes.fromhex("6C04")  # Le smaller than the UID
    assert apdu("FFB0000310") == bytes.fromhex("6982")  # not authenticated yet
    assert apdu("FF86000005" "0100036000") == bytes.fromhex("6982")  # key slot 0 empty
    assert apdu("FF82000006" + A_KEY.hex()) == bytes.fromhex("9000")
    assert apdu("FF86000005" "0100036000") == bytes.fromhex("9000")
    trailer = A_KEY + bytes([0xFF, 0x07, 0x80, 0x69]) + B_KEY
    assert apdu("FFB0000310") == trailer + bytes.fromhex("9000")
    assert apdu("FFB0000104") == bytes(range(16, 20)) + bytes.fromhex("9000")
    assert apdu("FFB0000410") == bytes.fromhex("6982")  # other sector
    new = bytes(range(0xC0, 0xD0))
    assert apdu("FFD6000110" + new.hex()) == bytes.fromhex("9000")
    assert apdu("FFB0000110") == new + bytes.fromhex("9000")
    assert apdu("FFD6000108" + "00" * 8) == bytes.fromhex("6700")  # must be 16 bytes
    # wrong key fails the authentication
    assert apdu("FF82000106" + "FF" * 6) == bytes.fromhex("9000")
    assert apdu("FF86000005" "0100036001") == bytes.fromhex("6300")
    assert apdu("FFB0000310") == bytes.fromhex("6982")  # failed auth drops the old one
    assert apdu("FF99000000") == bytes.fromhex("6A81")  # unknown pseudo-APDU
    assert apdu("00A4040000") == bytes.fromhex("6E00")  # real ISO7816 APDU, not an ISO-DEP card


def test_ccid_pcsc_ultralight():
    reset(UL)
    ccid(0x62)
    assert apdu("FFCA000000") == bytes.fromhex("04112233445566" "9000")
    assert apdu("FFB0000410") == bytes(range(0x50, 0x60)) + bytes.fromhex("9000")
    assert apdu("FFB0000404") == bytes(range(0x50, 0x54)) + bytes.fromhex("9000")
    assert apdu("FFD6000504DEADBEEF") == bytes.fromhex("9000")
    pg = (C.c_ubyte * 4)()
    lib.t_ul_page(5, pg)
    assert bytes(pg) == bytes.fromhex("DEADBEEF")
    assert apdu("FFD6000204" "00000000") == bytes.fromhex("6300")  # locked page
    assert apdu("FFD600050" "3" "000000") == bytes.fromhex("6700")  # not a multiple of 4


def test_ccid_iso14443_4_passthrough():
    reset(ISO4A)
    r = ccid(0x62)
    assert r[10:12] == b"\x3b\x87"
    assert apdu("00B00000") == bytes.fromhex("6985")
    assert apdu("00A4040006F00102030405") == bytes.fromhex("9000")
    assert apdu("00B00000") == b"HELO" + bytes.fromhex("9000")  # applet state survives between messages
    assert apdu("FFCA000000") == bytes.fromhex("08AABBCC9000")
    assert apdu("FFCA010000") == bytes.fromhex("80" "31C073C84000" "9000")  # historical bytes


def test_ccid_pn532_escape_for_libnfc_acr122_style_access():
    reset(MFC1K)
    ccid(0x62)
    assert apdu("FF00000004" "D44A0100") == bytes.fromhex("D54B01010004" "08" "04" "DEADBEEF" "9000")
    assert apdu("FF00000002D402") == bytes.fromhex("D503320106079000")  # GetFirmwareVersion
    assert apdu("FF00000002" "D48C") == bytes.fromhex("6300")  # unsupported PN532 command
    assert apdu("FF00000002" "D502") == bytes.fromhex("6A81")  # must start with D4


def test_ccid_slot_change_notifications():
    reset(MFC1K)
    note = (C.c_ubyte * 2)()
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x03])  # card present at start
    assert lib.t_ccid_notify(note) == 0
    lib.t_set_card(NONE)
    lib.t_set_time(100)
    assert lib.t_ccid_notify(note) == 0  # polling is throttled to 4/s
    lib.t_set_time(300)
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x02])
    lib.t_set_card(UL)
    lib.t_set_time(600)
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x03])
    # while a card is powered the engine must not poll (it would break the session)
    ccid(0x62)
    before = lib.t_stat(0)
    lib.t_set_time(5000)
    assert lib.t_ccid_notify(note) == 0 and lib.t_stat(0) == before


def test_ccid_parameters():
    reset(MFC1K)
    for t in (0x6C, 0x6D, 0x61):
        r = ccid(t)
        assert r[0] == 0x82 and (r[1] | r[2] << 8) == 7 and r[9] == 1


def test_ccid_card_removed_during_session():
    reset(ISO4A)
    ccid(0x62)
    lib.t_set_card(NONE)
    r = apdu_raw("00B00000")
    assert r[7] & 0x40 and r[8] == 0xFE
    assert ccid(0x65)[7] == 0x02  # now reported absent


# --------------------------------------------------------------- CCID helpers
_seq = [0]


def ccid(mtype, data=b"", slot=0):
    _seq[0] = (_seq[0] + 1) & 0xFF
    msg = bytes([mtype]) + len(data).to_bytes(4, "little") + bytes([slot, _seq[0], 0, 0, 0]) + data
    out = (C.c_ubyte * 300)()
    n = lib.t_ccid(buf(msg), len(msg), out, 300)
    r = bytes(out[:n])
    assert n >= 10, "no response"
    assert r[6] == _seq[0] and r[5] == slot
    assert int.from_bytes(r[1:5], "little") == n - 10
    return r


def apdu_raw(hexstr):
    return ccid(0x6F, bytes.fromhex(hexstr))


def apdu(hexstr):
    r = apdu_raw(hexstr)
    assert r[0] == 0x80 and r[7] == 0x00, "XfrBlock failed: %s" % r.hex()
    return r[10:]


def main():
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_") and callable(f)]
    failed = 0
    for name, fn in tests:
        try:
            fn()
            print("ok   ", name)
        except Exception as e:  # noqa
            failed += 1
            import traceback
            print("FAIL ", name, "-", repr(e))
            traceback.print_exc()
    print("\n%d tests, %d failed" % (len(tests), failed))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
