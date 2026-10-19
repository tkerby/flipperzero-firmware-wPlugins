#!/usr/bin/env python3
"""
Tests for the "load a dump / .nfc file and let the PC see it" feature (core/nb_virtual.c).

Each test builds a card file (Flipper .nfc text, MIFARE Classic Tool hex text, or a raw
binary dump), loads it into the virtual card, and then talks to it through the real
PN532 and CCID engines exactly like libnfc / pcscd would.

    cd nfc_bridge && make test
"""
import ctypes as C
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import test_core as T  # noqa: E402  (shares the same libnbtest.so and PN532/CCID helpers)

lib = T.lib
lib.tv_error.restype = C.c_char_p
lib.tv_name.restype = C.c_char_p

KEY_A = bytes.fromhex("A0A1A2A3A4A5")
KEY_B = bytes.fromhex("B0B1B2B3B4B5")
KEY_A2 = bytes.fromhex("112233445566")
KEY_B2 = bytes.fromhex("665544332211")
FF6 = b"\xff" * 6


# ------------------------------------------------------------------ builders
def bcc(uid4):
    x = 0
    for b in uid4:
        x ^= b
    return x


def make_ac(codes):
    """Access-bit bytes 6..9 for four groups (g0,g1,g2,trailer), each code = C1C2C3 as 0..7."""
    c1 = [(c >> 2) & 1 for c in codes]
    c2 = [(c >> 1) & 1 for c in codes]
    c3 = [c & 1 for c in codes]
    b6 = sum(((1 - c2[i]) << (4 + i)) | ((1 - c1[i]) << i) for i in range(4))
    b7 = sum((c1[i] << (4 + i)) | ((1 - c3[i]) << i) for i in range(4))
    b8 = sum((c3[i] << (4 + i)) | (c2[i] << i) for i in range(4))
    return bytes([b6, b7, b8, 0x69])


DEFAULT_AC = make_ac([0, 0, 0, 1])
assert DEFAULT_AC == bytes([0xFF, 0x07, 0x80, 0x69])


def sector_blocks(s):
    """Block numbers of sector s on a 4K card."""
    if s < 32:
        return list(range(s * 4, s * 4 + 4))
    first = 128 + (s - 32) * 16
    return list(range(first, first + 16))


def trailer(key_a=KEY_A, ac=DEFAULT_AC, key_b=KEY_B):
    return key_a + ac + key_b


def data_block(b):
    return bytes((b * 16 + i) & 0xFF for i in range(16))


def block0(uid4, sak=0x08, atqa=(0x04, 0x00)):
    return bytes(uid4) + bytes([bcc(uid4), sak, atqa[0], atqa[1]]) + bytes(range(0xA0, 0xA8))


def mfc_image(nblocks, uid4=b"\xDE\xAD\xBE\xEF", trailers=None):
    """List of 16-byte blocks. trailers: {sector: 16 bytes} overrides."""
    trailers = trailers or {}
    blocks = []
    for b in range(nblocks):
        if b == 0:
            blocks.append(block0(uid4))
        elif (b < 128 and b % 4 == 3) or (b >= 128 and (b - 128) % 16 == 15):
            s = b // 4 if b < 128 else 32 + (b - 128) // 16
            blocks.append(trailers.get(s, trailer()))
        else:
            blocks.append(data_block(b))
    return blocks


def fmt_hex(b, unk=None):
    return " ".join("??" if (unk and i in unk) else "%02X" % x for i, x in enumerate(b))


def flipper_mfc(blocks, typ="1K", uid="DE AD BE EF", atqa="00 04", sak="08", unknown=(), newline="\n", extra=""):
    """A Flipper .nfc file. `unknown` = block numbers written as all '??'; a block may also be
    given as (bytes, {byte indexes that are ??})."""
    out = [
        "Filetype: Flipper NFC device",
        "Version: 4",
        "# Device type can be ISO14443-3A, ISO14443-3B, NTAG/Ultralight, Mifare Classic ...",
        "Device type: Mifare Classic",
        "# UID is common for all formats",
        "UID: " + uid,
        "# ISO14443-3A specific data",
        "ATQA: " + atqa,
        "SAK: " + sak,
        "# Mifare Classic specific data",
        "Mifare Classic type: " + typ,
        "Data format version: 2",
        "# Mifare Classic blocks, '??' means unknown data",
    ]
    for n, b in enumerate(blocks):
        if n in unknown:
            out.append("Block %d: %s" % (n, " ".join(["??"] * 16)))
        elif isinstance(b, tuple):
            out.append("Block %d: %s" % (n, fmt_hex(b[0], b[1])))
        else:
            out.append("Block %d: %s" % (n, fmt_hex(b)))
    if extra:
        out.append(extra)
    return (newline.join(out) + newline).encode()


def ul_pages(npages, uid7=bytes.fromhex("04AABBCCDDEEFF")):
    pg = []
    for p in range(npages):
        if p == 0:
            pg.append(uid7[0:3] + bytes([0x88 ^ uid7[0] ^ uid7[1] ^ uid7[2]]))
        elif p == 1:
            pg.append(uid7[3:7])
        elif p == 2:
            pg.append(bytes([uid7[3] ^ uid7[4] ^ uid7[5] ^ uid7[6], 0x48, 0x00, 0x00]))
        else:
            pg.append(bytes((0x40 + p * 4 + i) & 0xFF for i in range(4)))
    return pg


def flipper_ul(pages, atqa="44 00", sak="00", uid="04 AA BB CC DD EE FF", dev="NTAG/Ultralight", total=None):
    out = [
        "Filetype: Flipper NFC device",
        "Version: 4",
        "Device type: " + dev,
        "UID: " + uid,
        "ATQA: " + atqa,
        "SAK: " + sak,
        "Signature: " + " ".join(["00"] * 32),
        "Mifare version: 00 04 04 02 01 00 11 03",
        "Counter 0: 0",
        "Tearing 0: 00",
        "Pages total: %d" % (len(pages) if total is None else total),
        "Pages read: %d" % len(pages),
    ]
    for n, p in enumerate(pages):
        out.append("Page %d: %s" % (n, fmt_hex(p)))
    return ("\n".join(out) + "\n").encode()


def mct_text(blocks, unknown_sectors=()):
    out = []
    for s in range(0, len(blocks) // 4 if len(blocks) <= 128 else 40):
        bl = sector_blocks(s)
        if bl[-1] >= len(blocks):
            break
        out.append("+Sector: %d" % s)
        for b in bl:
            out.append("-" * 32 if s in unknown_sectors else blocks[b].hex().upper())
    return ("\n".join(out) + "\n").encode()


# -------------------------------------------------------------------- helpers
def vload(data, chunk=0):
    ok = lib.tv_load(T.buf(data), len(data), chunk)
    if ok:
        lib.tv_attach()
    return bool(ok)


def verr():
    return lib.tv_error().decode()


def must_load(data, chunk=0):
    assert vload(data, chunk), "load failed: " + verr()


def expect_fail(data, text, chunk=0):
    assert not vload(data, chunk), "load unexpectedly worked"
    assert text.lower() in verr().lower(), "error was %r, wanted %r" % (verr(), text)


def auth(blk, key, b=False):
    return T.dex([0x61 if b else 0x60, blk], key, bytes(4))


def read(blk):
    return T.dex([0x30, blk])


def write(blk, data):
    return T.dex([0xA0, blk], data)


def read_ok(blk):
    r = read(blk)
    assert r[0] == 0x00, "read block %d failed: %s" % (blk, r.hex())
    return r[1:]


OK = bytes([0x00])
AUTH_ERR = bytes([0x14])


# ----------------------------------------------------------------------- tests
def test_flipper_classic_1k_through_pn532():
    img = mfc_image(64, trailers={1: trailer(KEY_A2, DEFAULT_AC, KEY_B2)})
    must_load(flipper_mfc(img))
    assert lib.tv_name() == b"MIFARE Classic 1K"
    # ATQA "00 04" in the file -> card sends 04 00 -> PN532 reports 00 04
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04, 0xDE, 0xAD, 0xBE, 0xEF])
    assert read(1) == AUTH_ERR  # not authenticated
    assert auth(3, FF6) == AUTH_ERR  # wrong key
    assert auth(3, KEY_A) == OK
    assert read_ok(1) == data_block(1)
    assert read_ok(0) == img[0]
    # trailer read: Key A is never given back, Key B is readable with default access bits
    assert read_ok(3) == bytes(6) + DEFAULT_AC + KEY_B
    # ... but not when authenticated with key B (the access bits are still returned)
    assert auth(3, KEY_B, True) == OK
    assert read_ok(3) == bytes(6) + DEFAULT_AC + bytes(6)
    # sector 1 has its own keys
    assert auth(4, KEY_A) == AUTH_ERR
    assert auth(4, KEY_A2) == OK
    assert read_ok(5) == data_block(5)
    assert auth(4, KEY_B2, True) == OK
    # writes go to the RAM image, and block 0 stays read-only
    assert auth(1, KEY_A) == OK
    new = bytes(range(0xC0, 0xD0))
    assert write(1, new) == OK
    assert read_ok(1) == new
    assert write(0, bytes(16)) != OK
    # releasing ends the session
    assert T.pn([0x52, 0x00]) == OK
    assert read(1) == bytes([0x27])


def test_loading_in_chunks_gives_the_same_card():
    img = mfc_image(64)
    data = flipper_mfc(img, newline="\r\n")  # CRLF too
    for chunk in (0, 1, 7, 63, 64, 65, 1000):
        must_load(data, chunk)
        assert T.list_target()[:6] == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04]), chunk
        assert auth(0, KEY_A) == OK and read_ok(2) == data_block(2), chunk
        assert lib.tv_name() == b"MIFARE Classic 1K"


def test_unknown_blocks_and_keys_behave_like_a_card_you_have_no_key_for():
    img = mfc_image(64)
    # sector 2 completely unknown; sector 3: Key B unknown ('??') and its data blocks unknown
    unknown = set(sector_blocks(2))
    img[15] = (trailer(), set(range(10, 16)))
    for b in (12, 13):
        unknown.add(b)
    must_load(flipper_mfc(img, unknown=unknown))
    T.list_target()
    assert auth(8, KEY_A) == AUTH_ERR and auth(8, KEY_B, True) == AUTH_ERR  # nothing known about it
    assert auth(12, KEY_B, True) == AUTH_ERR  # key B unknown: cannot be verified
    assert auth(12, KEY_A) == OK
    assert read(12)[0] != 0x00  # authenticates fine but the dump has no data for the block
    assert read(14)[0] == 0x00  # a block it does have
    # unknown Key B bytes are not returned as zeros-that-authenticate
    assert auth(12, bytes(6), True) == AUTH_ERR


def test_access_bits_are_enforced():
    # sector 5 (blocks 20-23): g0 = 011 (read/write key B only), g1 = 101 (read B, never write),
    #                          g2 = 111 (never), trailer = 001 (transport)
    ac = make_ac([3, 5, 7, 1])
    img = mfc_image(64, trailers={5: trailer(KEY_A, ac, KEY_B)})
    must_load(flipper_mfc(img))
    T.list_target()
    assert auth(20, KEY_A) == OK
    assert read(20) == AUTH_ERR  # key A may not read group 0
    assert auth(20, KEY_B, True) == OK
    assert read_ok(20) == data_block(20)
    assert write(20, bytes(16)) == OK  # key B may write group 0
    assert read_ok(20) == bytes(16)
    assert read_ok(21) == data_block(21)
    assert write(21, bytes(16)) != OK  # group 1 is never writable
    assert read(22) == AUTH_ERR  # group 2: never readable
    # transport trailer: key A can rewrite keys, key B cannot
    assert write(23, trailer(KEY_A, ac, KEY_B2)) != OK
    assert auth(23, KEY_A) == OK
    assert write(23, trailer(KEY_A, ac, KEY_B2)) == OK
    assert auth(20, KEY_B2, True) == OK
    assert auth(20, KEY_B, True) == AUTH_ERR  # the old key is gone
    # an invalid access-bit field would brick a real card, so the virtual one refuses it
    assert auth(23, KEY_A) == OK
    bad = KEY_A + bytes([0x00, 0x00, 0x00, 0x69]) + KEY_B2
    assert write(23, bad) != OK


def test_access_bits_that_lock_the_trailer():
    # trailer = 011: key A write / access bits need key B; key B not readable
    ac = make_ac([0, 0, 0, 3])
    img = mfc_image(64, trailers={2: trailer(KEY_A, ac, KEY_B)})
    must_load(flipper_mfc(img))
    T.list_target()
    assert auth(8, KEY_A) == OK
    assert read_ok(11) == bytes(6) + ac + bytes(6)  # B hidden
    assert write(11, trailer(KEY_A2, ac, KEY_B)) != OK  # key A may not change key A here
    assert auth(8, KEY_B, True) == OK
    assert write(11, trailer(KEY_A2, ac, KEY_B)) == OK
    assert auth(8, KEY_A2) == OK


def test_atqa_and_sak_are_normalised_so_the_card_looks_like_what_it_is():
    img = mfc_image(64)
    # the file says ATQA "04 00" (other byte order), SAK 28 (ISO-DEP flavoured): still a plain 1K
    must_load(flipper_mfc(img, atqa="04 00", sak="28"))
    assert T.list_target()[:6] == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04])
    # a 1K with SAK 88 (Infineon) keeps it
    must_load(flipper_mfc(img, sak="88"))
    assert T.list_target()[4] == 0x88
    # nonsense ATQA falls back to the proper one
    must_load(flipper_mfc(img, atqa="12 34"))
    assert T.list_target()[2:4] == bytes([0x00, 0x04])


def test_classic_4k_and_mini():
    img = mfc_image(256, trailers={32: trailer(KEY_A2, DEFAULT_AC, KEY_B2)})
    must_load(flipper_mfc(img, typ="4K", atqa="00 02", sak="18"))
    assert lib.tv_name() == b"MIFARE Classic 4K"
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x02, 0x18, 0x04, 0xDE, 0xAD, 0xBE, 0xEF])
    assert auth(130, KEY_A2) == OK  # first 16-block sector
    assert read_ok(131) == data_block(131)
    assert read_ok(143) == bytes(6) + DEFAULT_AC + KEY_B2
    assert auth(255, KEY_A) == OK
    assert read_ok(254) == data_block(254)
    # mini
    must_load(flipper_mfc(mfc_image(20), typ="Mini", atqa="00 04", sak="09"))
    assert lib.tv_name() == b"MIFARE Classic Mini"
    assert T.list_target()[4] == 0x09
    assert auth(16, KEY_A) == OK and read_ok(17) == data_block(17)
    assert auth(20, KEY_A) != OK  # there is no sector 5 on a Mini


def test_flipper_ntag215_through_pn532():
    pages = ul_pages(135)
    must_load(flipper_ul(pages))
    assert lib.tv_name() == b"NTAG215"
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x44, 0x00, 0x07]) + bytes.fromhex("04AABBCCDDEEFF")
    assert T.dex([0x30, 0x00]) == OK + pages[0] + pages[1] + pages[2] + pages[3]
    assert T.dex([0x30, 0x04]) == OK + b"".join(pages[4:8])
    assert T.dex([0x30, 0x86]) == OK + pages[134] + pages[0] + pages[1] + pages[2]  # rolls over
    assert T.dex([0x30, 135])[0] != 0x00  # past the end: the tag NAKs
    assert T.dex([0xA2, 0x0A], bytes.fromhex("DEADBEEF")) == OK
    assert T.dex([0x30, 0x0A])[1:5] == bytes.fromhex("DEADBEEF")
    assert T.dex([0xA2, 0x02], bytes(4)) != OK  # UID / lock page
    assert T.dex([0xA2, 135], bytes(4)) != OK
    # page 3 is the OTP area: bits can only be set
    before = T.dex([0x30, 0x03])[1:5]
    assert T.dex([0xA2, 0x03], bytes.fromhex("00000F00")) == OK
    after = T.dex([0x30, 0x03])[1:5]
    assert after == bytes(a | b for a, b in zip(before, bytes.fromhex("00000F00")))
    assert T.dex([0xA2, 0x03], bytes(4)) == OK
    assert T.dex([0x30, 0x03])[1:5] == after  # writing zeros changes nothing


def test_ultralight_variants_atqa_order_and_names():
    for atqa in ("44 00", "00 44"):  # both byte orders occur in the wild
        must_load(flipper_ul(ul_pages(16), atqa=atqa, dev="Mifare Ultralight"))
        assert T.list_target()[2:5] == bytes([0x00, 0x44, 0x00]), atqa
    assert lib.tv_name() == b"MIFARE Ultralight"
    must_load(flipper_ul(ul_pages(45)))
    assert lib.tv_name() == b"NTAG213"
    must_load(flipper_ul(ul_pages(231)))
    assert lib.tv_name() == b"NTAG216"
    # old-style device type "NTAG215" is accepted too
    must_load(flipper_ul(ul_pages(135), dev="NTAG215"))
    assert lib.tv_name() == b"NTAG215"
    # no "Pages total": taken from the pages present
    data = flipper_ul(ul_pages(20)).replace(b"Pages total: 20\n", b"")
    must_load(data)
    assert lib.tv_name() == b"Ultralight EV1 (11)"
    # SAK in the file does not matter: it is always 00 for these
    must_load(flipper_ul(ul_pages(16), sak="20"))
    assert T.list_target()[4] == 0x00


def test_pcsc_sees_the_virtual_classic_card():
    img = mfc_image(64)
    must_load(flipper_mfc(img))
    r = T.ccid(0x65)
    assert r[0] == 0x81 and r[7] == 0x02  # nothing reported yet
    note = (C.c_ubyte * 2)()
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x03])  # card inserted
    r = T.ccid(0x62)  # power on
    assert r[0] == 0x80 and r[7] == 0x00
    atr = r[10:10 + (r[1] | r[2] << 8)]
    assert atr.hex() == "3b8f8001804f0ca0000003060300010000000" "06a", atr.hex()  # "MIFARE Classic 1K"
    assert T.apdu("FFCA000000") == bytes.fromhex("DEADBEEF9000")
    assert T.apdu("FF82000006" + KEY_A.hex()) == bytes.fromhex("9000")
    assert T.apdu("FF86000005" "0100006000") == bytes.fromhex("9000")
    assert T.apdu("FFB0000110") == data_block(1) + bytes.fromhex("9000")
    assert T.apdu("FFB0000310") == bytes(6) + DEFAULT_AC + KEY_B + bytes.fromhex("9000")
    assert T.apdu("FFB0000410") == bytes.fromhex("6982")  # sector 1 not authenticated
    assert T.apdu("FF82000106" + "FF" * 6) == bytes.fromhex("9000")
    assert T.apdu("FF86000005" "0100046001") == bytes.fromhex("6300")  # wrong key


def test_ppss_request_is_echoed_so_libccid_can_connect():
    must_load(flipper_mfc(mfc_image(64)))
    note = (C.c_ubyte * 2)()
    lib.t_ccid_notify(note)
    r = T.ccid(0x62)
    assert r[0] == 0x80 and r[7] == 0x00
    for pps in ("FF1111FF", "FF00FF", "FF119678"):  # what libccid sends after power-on
        r = T.ccid(0x6F, bytes.fromhex(pps))
        assert r[0] == 0x80 and r[7] == 0x00, r.hex()
        assert r[10:10 + (r[1] | r[2] << 8)].hex() == pps.lower()
    # the pseudo-APDUs next to it still work
    assert T.apdu("FFCA000000") == bytes.fromhex("DEADBEEF9000")


def test_pcsc_sees_the_virtual_ntag():
    pages = ul_pages(135)
    must_load(flipper_ul(pages))
    note = (C.c_ubyte * 2)()
    assert lib.t_ccid_notify(note) == 2
    r = T.ccid(0x62)
    atr = r[10:10 + (r[1] | r[2] << 8)]
    assert atr[12:16].hex() == "03000300", atr.hex()  # name 0003 = Ultralight/NTAG
    assert T.apdu("FFCA000000") == bytes.fromhex("04AABBCCDDEEFF9000")
    assert T.apdu("FFB0000410") == b"".join(pages[4:8]) + bytes.fromhex("9000")
    assert T.apdu("FFD6000504CAFEBABE") == bytes.fromhex("9000")
    assert T.apdu("FFB0000504") == bytes.fromhex("CAFEBABE9000")
    # libnfc over PC/SC (acr122_pcsc style): InListPassiveTarget through the escape
    assert T.apdu("FF00000004" "D44A0100") == bytes.fromhex("D54B01010044" "00" "07" "04AABBCCDDEEFF" "9000")


def test_raw_binary_classic_dumps():
    img = mfc_image(64)
    dump = b"".join(img)
    assert len(dump) == 1024
    must_load(dump)
    assert lib.tv_name() == b"MIFARE Classic 1K"
    # UID, SAK and ATQA come from block 0 (04 00 on the air -> PN532 reports 00 04)
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04, 0xDE, 0xAD, 0xBE, 0xEF])
    assert auth(7, KEY_A) == OK and read_ok(6) == data_block(6)
    # chunked loading of a binary file
    must_load(dump, chunk=100)
    T.list_target()
    assert auth(7, KEY_A) == OK and read_ok(6) == data_block(6)
    # 4K and Mini by size
    must_load(b"".join(mfc_image(256)))
    assert lib.tv_name() == b"MIFARE Classic 4K"
    assert T.list_target()[2:5] == bytes([0x00, 0x02, 0x18])
    must_load(b"".join(mfc_image(20)))
    assert lib.tv_name() == b"MIFARE Classic Mini"
    # a 7-byte UID (block 0 starts with the UID, SAK at byte 7, ATQA at 8-9)
    uid7 = bytes.fromhex("04112233445566")
    b0 = uid7 + bytes([0x08, 0x44, 0x00]) + bytes(6)
    img7 = mfc_image(64)
    img7[0] = b0
    must_load(b"".join(img7))
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x44, 0x08, 0x07]) + uid7


def test_raw_binary_ntag_dump():
    pages = ul_pages(135)
    dump = b"".join(pages)
    assert len(dump) == 540
    must_load(dump)
    assert lib.tv_name() == b"NTAG215"
    assert T.list_target() == bytes([0x01, 0x01, 0x00, 0x44, 0x00, 0x07]) + bytes.fromhex("04AABBCCDDEEFF")
    assert T.dex([0x30, 0x04]) == OK + b"".join(pages[4:8])
    must_load(b"".join(ul_pages(16)))
    assert lib.tv_name() == b"MIFARE Ultralight"
    must_load(b"".join(ul_pages(45)))
    assert lib.tv_name() == b"NTAG213"


def test_mifare_classic_tool_hex_text_dump():
    img = mfc_image(64, trailers={1: trailer(KEY_A2, DEFAULT_AC, KEY_B2)})
    must_load(mct_text(img))
    assert lib.tv_name() == b"MIFARE Classic 1K"
    assert T.list_target()[:6] == bytes([0x01, 0x01, 0x00, 0x04, 0x08, 0x04])
    assert auth(4, KEY_A2) == OK and read_ok(6) == data_block(6)
    # '-' lines = sector the tool could not read
    must_load(mct_text(img, unknown_sectors={3}))
    T.list_target()
    assert auth(12, KEY_A) == AUTH_ERR
    assert auth(8, KEY_A) == OK
    # a dump without the "+Sector" lines is accepted as well
    plain = ("\n".join(b.hex() for b in img) + "\n").encode()
    must_load(plain)
    T.list_target()
    assert auth(0, KEY_A) == OK and read_ok(1) == data_block(1)
    # the same with the file chopped into awkward pieces
    must_load(plain, chunk=33)
    T.list_target()
    assert auth(0, KEY_A) == OK
    # wrong number of blocks
    expect_fail(("\n".join(b.hex() for b in img[:50]) + "\n").encode(), "block count")


def test_uid_only_card():
    text = (
        "Filetype: Flipper NFC device\nVersion: 4\nDevice type: ISO14443-3A\n"
        "UID: 04 11 22 33 44 55 66\nATQA: 44 00\nSAK: 00\n"
    ).encode()
    must_load(text)
    assert lib.tv_name() == b"UID-only card"
    assert T.list_target()[:6] == bytes([0x01, 0x01, 0x00, 0x44, 0x00, 0x07])
    assert T.dex([0x30, 0x04])[0] != 0x00  # no data to read
    r = T.ccid(0x62)
    assert r[7] == 0x00
    assert T.apdu("FFCA000000") == bytes.fromhex("04112233445566" "9000")


def test_files_that_cannot_be_used_say_why():
    expect_fail(b"", "empty")
    expect_fail(b"hello world\n" * 50, "text")
    expect_fail(bytes(range(1, 18)), "too small")
    expect_fail(b"\x04" + bytes(2047), "2K")  # Classic 2K dump: not supported
    expect_fail(b"\x04" + bytes(5000), "too large")
    des = b"Filetype: Flipper NFC device\nVersion: 4\nDevice type: Mifare DESFire\nUID: 04 11 22 33 44 55 66\n"
    expect_fail(des, "Unsupported: Mifare DESFire")
    expect_fail(b"Filetype: Something else\nDevice type: Mifare Classic\n", "not a Flipper")
    expect_fail(b"Filetype: Flipper NFC device\nVersion: 4\n", "device type")
    # block outside the declared card size
    img = mfc_image(20)
    extra = "Block 40: " + " ".join(["00"] * 16)
    expect_fail(flipper_mfc(img, typ="Mini", extra=extra), "beyond")
    # a block with the wrong number of bytes
    bad = flipper_mfc(mfc_image(64)).replace(b"Block 5: ", b"Block 5: 00 ")
    expect_fail(bad, "block 5")
    # no UID anywhere
    nouid = flipper_mfc(mfc_image(64)).replace(b"UID: DE AD BE EF\n", b"")
    must_load(nouid)  # block 0 still has it
    assert T.list_target()[6:] == bytes([0xDE, 0xAD, 0xBE, 0xEF])
    nouid2 = flipper_mfc(mfc_image(64), unknown={0}).replace(b"UID: DE AD BE EF\n", b"")
    expect_fail(nouid2, "UID unknown")
    # once an error happened the loader stays failed until the next load_begin
    lib.tv_load(T.buf(b"junk"), 4, 0)
    assert not vload(b"")
    # and a good load afterwards works again
    must_load(flipper_mfc(mfc_image(64)))


def test_comments_bom_and_unrelated_lines_are_ignored():
    data = b"\xef\xbb\xbf" + flipper_mfc(mfc_image(64), extra="Some future field: whatever\n# comment")
    must_load(data)
    assert T.list_target()[:2] == bytes([0x01, 0x01])
    # a very long unrelated line is skipped, a long Block line is an error
    must_load(flipper_mfc(mfc_image(64), extra="Notes: " + "x" * 400))
    expect_fail(flipper_mfc(mfc_image(64), extra="Block 7: " + "00 " * 100), "too long")


def test_removing_and_reinserting_the_card_pn532():
    must_load(flipper_mfc(mfc_image(64)))
    assert T.list_target()[0] == 0x01
    assert auth(0, KEY_A) == OK
    lib.tv_set_present(0)
    assert T.list_target() == bytes([0x00])  # nothing on the reader
    assert auth(0, KEY_A) != OK
    lib.tv_set_present(1)
    assert T.list_target()[0] == 0x01
    assert auth(0, KEY_A) == OK and read_ok(1) == data_block(1)


def test_removing_and_reinserting_the_card_pcsc():
    must_load(flipper_mfc(mfc_image(64)))
    note = (C.c_ubyte * 2)()
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x03])
    T.ccid(0x62)  # host powers the card
    assert T.apdu("FFCA000000").endswith(bytes.fromhex("9000"))
    lib.tv_set_present(0)
    # the engine must notice although the card is powered (that is what is_present is for)
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x02])
    assert T.ccid(0x65)[7] == 0x02
    r = T.apdu_raw("FFCA000000")
    assert r[7] & 0x40  # command fails: no card
    lib.tv_set_present(1)
    lib.tv_set_time(300)
    assert lib.t_ccid_notify(note) == 2 and bytes(note) == bytes([0x50, 0x03])
    r = T.ccid(0x62)
    assert r[7] == 0x00
    assert T.apdu("FFCA000000") == bytes.fromhex("DEADBEEF9000")


def test_reloading_forgets_changes_made_through_the_bridge():
    data = flipper_mfc(mfc_image(64))
    must_load(data)
    T.list_target()
    assert auth(0, KEY_A) == OK and write(1, bytes(16)) == OK and read_ok(1) == bytes(16)
    must_load(data)
    T.list_target()
    assert auth(0, KEY_A) == OK and read_ok(1) == data_block(1)


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
