#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui_h = (root / "src/ui/ui_manager.h").read_text()
ui = (root / "src/ui/ui_manager.cpp").read_text()
writer = (root / "src/magic/magic_writer.cpp").read_text()
dictionary = (root / "src/bambu_dictionary.cpp").read_text()
reader = (root / "src/bambu_reader.cpp").read_text()

assert "Screen : uint8_t { MainMenu, Advanced" in ui_h
assert "Wipe Bambu magic tag" in ui
assert "draw_wrapped_text_scrolled" in ui
assert "write_offset_" in ui and "write_scroll_max_" in ui
assert "wipe_offset_" in ui and "wipe_scroll_max_" in ui
assert "br_wipe_worker" in writer
assert "br_derive_sector_keys" in writer
assert "Verifying all 64 blank blocks and default keys" in writer

# Native Flipper user dictionary, but one batch update after UID collection.
assert 'EXT_PATH("nfc/assets/mf_classic_dict_user.nfc")' in dictionary
assert "br_add_uid_batch_to_user_dictionary" in dictionary
assert "keys_dict_get_total_keys" in dictionary
assert "keys_dict_get_next_key" in dictionary
assert "keys_dict_add_key" in dictionary
assert "keys_dict_is_key_present" not in dictionary
assert "br_derive_sector_keys(uids[uid_index]" in dictionary
assert "br_add_device_keys_to_user_dictionary" not in reader
assert "br_add_dump_keys_to_user_dictionary" not in ui
assert "queueSavedUidsForDictionary" in ui
assert "saved_[index].info.uid" in ui
assert 'furi_thread_alloc_ex(\n        "BambuDict"' in ui
assert "DictionaryDone" in ui_h and "DictionaryDone" in ui

# RGB updates must never block the ViewDispatcher/input callback.
assert "NotificationMessageTypeLedRed" in ui
assert "NotificationMessageTypeLedGreen" in ui
assert "NotificationMessageTypeLedBlue" in ui
assert "notification_message(notification_, &BrTagLedSequence)" in ui
assert "notification_message_block(notification_" not in ui
assert (
    "info.color_rgba[0]" in ui
    and "info.color_rgba[1]" in ui
    and "info.color_rgba[2]" in ui
)
assert "screen() == Screen::Browse" in ui and "screen() == Screen::Detail" in ui
print("Bambu UI/wipe/LED/batched-dictionary structure OK")
