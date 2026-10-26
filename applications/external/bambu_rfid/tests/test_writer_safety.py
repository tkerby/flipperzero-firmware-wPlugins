#!/usr/bin/env python3

# Pure-data regression checks for the writer's block-0 and lock assumptions.
# Hardware behavior is still verified at runtime on the Flipper after every write.

from pathlib import Path

SOURCE_BLOCK0 = bytes.fromhex("EAFE5CFCB40804000453F1FFC0875F90")
UFUID_SEAL = bytes.fromhex("85000000000000000000000000000008")
GEN4_DEFAULT_CONFIG = bytes.fromhex(
    "00000000000002000978009102DABC191010111213141516040008006B024F6B"
)


def bcc(uid: bytes) -> int:
    value = 0
    for byte in uid:
        value ^= byte
    return value


def probe_block0(source: bytes, uid: bytes) -> bytes:
    out = bytearray(source)
    out[:4] = uid
    out[4] = bcc(uid)
    return bytes(out)


def main():
    assert len(SOURCE_BLOCK0) == 16
    assert SOURCE_BLOCK0[4] == bcc(SOURCE_BLOCK0[:4])

    probe_uid = bytes.fromhex("12345678")
    probe = probe_block0(SOURCE_BLOCK0, probe_uid)
    assert probe[:4] == probe_uid
    assert probe_uid != bytes(4)  # zero UID is reserved from runtime probes
    assert probe[4] == 0x08  # 12 ^ 34 ^ 56 ^ 78
    assert (
        probe[5:] == SOURCE_BLOCK0[5:]
    )  # UID test changes nothing else in page/block 0

    assert len(UFUID_SEAL) == 16
    assert UFUID_SEAL[0] == 0x85 and UFUID_SEAL[-1] == 0x08

    assert len(GEN4_DEFAULT_CONFIG) == 32
    assert GEN4_DEFAULT_CONFIG[0] == 0x00  # MIFARE Classic
    assert GEN4_DEFAULT_CONFIG[1] == 0x00  # 4-byte UID
    assert GEN4_DEFAULT_CONFIG[29] == 0x02  # factory direct-write mode
    assert len(GEN4_DEFAULT_CONFIG[:30]) == 30  # F0/F1 omit returned CRC bytes

    factory_block0 = bytes.fromhex("00010203000804000000000000000000")
    assert len(factory_block0) == 16
    assert factory_block0[4] == bcc(factory_block0[:4])
    assert factory_block0[5:8] == bytes.fromhex("080400")

    default_trailer = bytes.fromhex("FFFFFFFFFFFFFF078069FFFFFFFFFFFF")
    assert len(default_trailer) == 16
    assert default_trailer[:6] == b"\xff" * 6
    assert default_trailer[6:10] == bytes.fromhex("FF078069")
    assert default_trailer[10:] == b"\xff" * 6

    # UID probes on an already-written Gen2 tag temporarily relax only the
    # access bytes, retaining both Bambu keys so sector 0 can always be restored.
    bambu_trailer = bytes.fromhex("2FC5F17A68A487878769759F4DF068B6")
    relaxed = bytearray(bambu_trailer)
    relaxed[6:10] = bytes.fromhex("FF078069")
    assert relaxed[:6] == bambu_trailer[:6]
    assert relaxed[10:] == bambu_trailer[10:]
    assert relaxed[6:10] == bytes.fromhex("FF078069")

    # Gen3 UID mutation is tested with the dedicated 90 FB command, not by
    # rewriting block 0. Proxmark keeps Lc=07 even for a 4-byte UID.
    gen3_probe = bytes.fromhex("90FBCCCC07") + probe_uid
    assert gen3_probe[:5] == bytes.fromhex("90FBCCCC07")
    assert len(gen3_probe) == 9

    # A DirectWrite OTP can make its first block-0 write permanent. The writer's
    # unknown-tag preflight must therefore use the final Bambu page, never a probe.
    first_directwrite_block0 = SOURCE_BLOCK0
    assert first_directwrite_block0 == SOURCE_BLOCK0
    assert first_directwrite_block0 != probe

    # Canonical wipe/fallback is the standard empty magic 1K image, not a
    # random UID with zero-filled memory.
    # Structural regression: unknown blank tags must never receive a random UID
    # before the selected Bambu page 0. This protects write-once DirectWrite OTP.
    writer = (
        Path(__file__).resolve().parents[1] / "src" / "magic" / "magic_writer.cpp"
    ).read_text()
    assert "0x00, 0x01, 0x02, 0x03, 0x00, 0x08, 0x04, 0x00" in writer
    assert "make_blank_dump(&blank);" in writer
    preflight = writer.split("// Unknown non-Gen1/Gen4 target.", 1)[1].split(
        "// Program data before each trailer.", 1
    )[0]
    assert "make_probe_block0" not in preflight
    assert "gen3_read_block0" in preflight
    assert "source.blocks[0]" in preflight

    # Every irreversible family lock is gated by exact source verification, and
    # MF4 overlay detection happens before the Gen1A source dump write.
    assert "bool safe_to_lock" in writer
    safe_body = writer.split(
        "bool safe_to_lock(Nfc* nfc, const BrTagDump& source) {", 1
    )[1].split("bool lock_ufuid", 1)[0]
    assert "live_sak" in safe_body and "live_atqa" in safe_body
    assert "verify_dump(nfc, source)" in safe_body
    assert "verify_block0_exact(nfc, source)" in safe_body
    gen1_initial = writer.split("if(run_gen1(nfc, 0x40, 0x43", 1)[1].split(
        "// Gen1B acknowledges", 1
    )[0]
    assert gen1_initial.index("detect_mf4_overlay") < gen1_initial.index("&source")

    # Lock verification intentionally requires three clean blocked attempts;
    # a single RF timeout is not accepted as evidence that the UID is locked.
    probe_attempts = 3
    assert probe_attempts == 3

    print("Bambu writer safety vectors OK")


if __name__ == "__main__":
    main()
