#include "magic_writer.h"

#include "../bambu_tag.h"
#include "../bambu_crypto.h"

#include <furi_hal_random.h>
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <toolbox/bit_buffer.h>

#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t RawFwt = 200000U;
constexpr uint32_t WriterDoneFlag = (1UL << 0);
constexpr uint8_t DefaultKeyBytes[MF_CLASSIC_KEY_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint8_t UfuidSealConfig[BR_BLOCK_SIZE] = {
    0x85,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
};

struct Gen1Session {
    Nfc* nfc;
    FuriThreadId thread_id;
    BitBuffer* tx;
    BitBuffer* rx;
    uint8_t wake;
    uint8_t access;
    const BrTagDump* dump;
    const uint8_t* one_block;
    const uint8_t* config_out_source;
    uint8_t* config_in_dest;
    uint8_t* read_block_dest;
    uint8_t read_block_num;
    bool detect_only;
    bool read_config;
    bool read_one_block;
    bool write_config;
    bool write_one_block;
    bool write_dump;
    bool skip_dump_block0;
    bool ok;
};

struct SelectedSession {
    FuriThreadId thread_id;
    BitBuffer* tx;
    BitBuffer* rx;
    const uint8_t* command;
    size_t command_len;
    uint8_t* response;
    size_t response_capacity;
    size_t response_len;
    bool ok;
};

bool cancelled(const BrWriteContext* ctx) {
    return ctx && ctx->cancel && *ctx->cancel;
}

bool cancelled(const BrWipeContext* ctx) {
    return ctx && ctx->cancel && *ctx->cancel;
}

void report_common(
    ViewDispatcher* dispatcher,
    uint32_t progress_event,
    BrWriteResult* result,
    const char* message) {
    if(!result) return;
    snprintf(result->message, sizeof(result->message), "%s", message ? message : "");
    if(dispatcher) view_dispatcher_send_custom_event(dispatcher, progress_event);
}

void report(BrWriteContext* ctx, const char* message) {
    if(!ctx) return;
    report_common(ctx->dispatcher, ctx->progress_event, ctx->result, message);
}

void report(BrWipeContext* ctx, const char* message) {
    if(!ctx) return;
    report_common(ctx->dispatcher, ctx->progress_event, ctx->result, message);
}

bool ack4(const BitBuffer* rx) {
    return rx && bit_buffer_get_size(rx) == 4U && bit_buffer_get_byte(rx, 0) == 0x0AU;
}

void key_from_bytes(const uint8_t bytes[6], MfClassicKey* key);

bool gen1_tx(Gen1Session* s, const uint8_t* data, size_t bytes, size_t bits, bool crc) {
    bit_buffer_reset(s->tx);
    bit_buffer_reset(s->rx);
    if(bits)
        bit_buffer_copy_bits(s->tx, data, bits);
    else
        bit_buffer_copy_bytes(s->tx, data, bytes);
    if(crc) iso14443_crc_append(Iso14443CrcTypeA, s->tx);
    return nfc_poller_trx(s->nfc, s->tx, s->rx, RawFwt) == NfcErrorNone;
}

bool gen1_write_block_in_session(Gen1Session* s, uint8_t block, const uint8_t data[BR_BLOCK_SIZE]) {
    const uint8_t write_cmd[2] = {0xA0, block};
    if(!gen1_tx(s, write_cmd, sizeof(write_cmd), 0, true) || !ack4(s->rx)) return false;
    return gen1_tx(s, data, BR_BLOCK_SIZE, 0, true) && ack4(s->rx);
}

NfcCommand gen1_callback(NfcEvent event, void* context) {
    Gen1Session* s = static_cast<Gen1Session*>(context);
    if(event.type != NfcEventTypePollerReady) return NfcCommandContinue;

    s->ok = false;
    do {
        const uint8_t wake[1] = {s->wake};
        if(!gen1_tx(s, wake, 0, 7, false) || !ack4(s->rx)) break;

        // Gen1B skips the second magic-access byte. For detection, require the
        // access byte when one is specified so Gen1B is not misclassified as
        // Gen1A merely because both acknowledge the 7-bit 0x40 wakeup.
        if(s->access != 0U) {
            const uint8_t access[1] = {s->access};
            if(!gen1_tx(s, access, 1, 0, false) || !ack4(s->rx)) break;
        }
        if(s->detect_only) {
            s->ok = true;
            break;
        }

        if(s->read_one_block) {
            const uint8_t cmd[2] = {0x30, s->read_block_num};
            if(!gen1_tx(s, cmd, sizeof(cmd), 0, true)) break;
            if(bit_buffer_get_size_bytes(s->rx) != BR_BLOCK_SIZE + 2U) break;
            bit_buffer_write_bytes(s->rx, s->read_block_dest, BR_BLOCK_SIZE);
            s->ok = true;
            break;
        }

        if(s->read_config) {
            const uint8_t cmd[2] = {0xE0, 0x00};
            if(!gen1_tx(s, cmd, sizeof(cmd), 0, true)) break;
            if(bit_buffer_get_size_bytes(s->rx) < BR_BLOCK_SIZE) break;
            bit_buffer_write_bytes(s->rx, s->config_in_dest, BR_BLOCK_SIZE);
            s->ok = true;
            break;
        }

        if(s->write_config) {
            const uint8_t cmd[2] = {0xE1, 0x00};
            if(!gen1_tx(s, cmd, sizeof(cmd), 0, true) || !ack4(s->rx)) break;
            if(!gen1_tx(s, s->config_out_source, BR_BLOCK_SIZE, 0, true) || !ack4(s->rx)) break;
            s->ok = true;
            break;
        }

        if(s->write_one_block) {
            s->ok = gen1_write_block_in_session(s, 0, s->one_block);
            break;
        }

        if(s->write_dump) {
            // Block 0 is deliberately last. Some OTP/FUID/MF4-style tags disable
            // their magic path as soon as the manufacturer block is personalized.
            bool ok = true;
            for(uint8_t block = 1; block < BR_BLOCK_COUNT; ++block) {
                if(!gen1_write_block_in_session(s, block, s->dump->blocks[block])) {
                    ok = false;
                    break;
                }
            }
            if(ok && !s->skip_dump_block0)
                ok = gen1_write_block_in_session(s, 0, s->dump->blocks[0]);
            s->ok = ok;
            break;
        }
    } while(false);

    furi_thread_flags_set(s->thread_id, WriterDoneFlag);
    return NfcCommandStop;
}

bool run_gen1(
    Nfc* nfc,
    uint8_t wake,
    uint8_t access,
    const BrTagDump* dump,
    const uint8_t* one_block,
    uint8_t* config_in,
    const uint8_t* config_out,
    bool detect_only,
    bool skip_dump_block0 = false) {
    Gen1Session s{};
    s.nfc = nfc;
    s.thread_id = furi_thread_get_current_id();
    s.tx = bit_buffer_alloc(64);
    s.rx = bit_buffer_alloc(64);
    s.wake = wake;
    s.access = access;
    s.dump = dump;
    s.one_block = one_block;
    s.config_in_dest = config_in;
    s.config_out_source = config_out;
    s.detect_only = detect_only;
    s.read_config = config_in != nullptr;
    s.write_config = config_out != nullptr;
    s.write_one_block = one_block != nullptr;
    s.write_dump = dump != nullptr;
    s.skip_dump_block0 = skip_dump_block0;

    if(!s.tx || !s.rx) {
        if(s.tx) bit_buffer_free(s.tx);
        if(s.rx) bit_buffer_free(s.rx);
        return false;
    }

    nfc_config(nfc, NfcModePoller, NfcTechIso14443a);
    nfc_set_guard_time_us(nfc, ISO14443_3A_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(nfc, ISO14443_3A_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(nfc, ISO14443_3A_POLL_POLL_MIN_US);
    nfc_start(nfc, gen1_callback, &s);
    const uint32_t flags =
        furi_thread_flags_wait(WriterDoneFlag, FuriFlagWaitAny, FuriWaitForever);
    if(flags & WriterDoneFlag) furi_thread_flags_clear(WriterDoneFlag);
    nfc_stop(nfc);

    bit_buffer_free(s.tx);
    bit_buffer_free(s.rx);
    return s.ok;
}

bool run_gen1_read_block(
    Nfc* nfc,
    uint8_t wake,
    uint8_t access,
    uint8_t block_num,
    uint8_t out[BR_BLOCK_SIZE]) {
    Gen1Session s{};
    s.nfc = nfc;
    s.thread_id = furi_thread_get_current_id();
    s.tx = bit_buffer_alloc(64);
    s.rx = bit_buffer_alloc(64);
    s.wake = wake;
    s.access = access;
    s.read_block_dest = out;
    s.read_block_num = block_num;
    s.read_one_block = true;

    if(!s.tx || !s.rx) {
        if(s.tx) bit_buffer_free(s.tx);
        if(s.rx) bit_buffer_free(s.rx);
        return false;
    }

    nfc_config(nfc, NfcModePoller, NfcTechIso14443a);
    nfc_set_guard_time_us(nfc, ISO14443_3A_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(nfc, ISO14443_3A_FDT_POLL_FC);
    nfc_set_fdt_poll_poll_us(nfc, ISO14443_3A_POLL_POLL_MIN_US);
    nfc_start(nfc, gen1_callback, &s);
    const uint32_t flags =
        furi_thread_flags_wait(WriterDoneFlag, FuriFlagWaitAny, FuriWaitForever);
    if(flags & WriterDoneFlag) furi_thread_flags_clear(WriterDoneFlag);
    nfc_stop(nfc);

    bit_buffer_free(s.tx);
    bit_buffer_free(s.rx);
    return s.ok;
}

bool detect_mf4_overlay(Nfc* nfc, bool* detected) {
    *detected = false;
    uint8_t backdoor[BR_BLOCK_SIZE]{};
    if(!run_gen1_read_block(nfc, 0x40, 0x43, 3, backdoor)) return false;

    MfClassicBlock normal{};
    MfClassicKey key_a{};
    MfClassicKey key_b{};
    key_from_bytes(backdoor, &key_a);
    key_from_bytes(backdoor + 10, &key_b);

    MfClassicError error =
        mf_classic_poller_sync_read_block(nfc, 3, &key_a, MfClassicKeyTypeA, &normal);
    if(error != MfClassicErrorNone)
        error = mf_classic_poller_sync_read_block(nfc, 3, &key_b, MfClassicKeyTypeB, &normal);
    if(error != MfClassicErrorNone) return false;

    *detected = memcmp(backdoor + 6, normal.data + 6, 4) != 0;
    return true;
}

NfcCommand selected_callback(NfcGenericEvent event, void* context) {
    SelectedSession* s = static_cast<SelectedSession*>(context);
    if(event.protocol != NfcProtocolIso14443_3a || !event.event_data || !event.instance)
        return NfcCommandContinue;

    Iso14443_3aPollerEvent* iso_event = static_cast<Iso14443_3aPollerEvent*>(event.event_data);
    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        bit_buffer_reset(s->tx);
        bit_buffer_reset(s->rx);
        bit_buffer_copy_bytes(s->tx, s->command, s->command_len);
        const Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
            static_cast<Iso14443_3aPoller*>(event.instance), s->tx, s->rx, RawFwt);
        if(error == Iso14443_3aErrorNone) {
            s->response_len = bit_buffer_get_size_bytes(s->rx);
            if(s->response && s->response_capacity) {
                const size_t copy = s->response_len < s->response_capacity ? s->response_len :
                                                                             s->response_capacity;
                bit_buffer_write_bytes(s->rx, s->response, copy);
            }
            s->ok = true;
        }
        furi_thread_flags_set(s->thread_id, WriterDoneFlag);
        return NfcCommandStop;
    }

    if(iso_event->type == Iso14443_3aPollerEventTypeError) {
        furi_thread_flags_set(s->thread_id, WriterDoneFlag);
        return NfcCommandStop;
    }
    return NfcCommandContinue;
}

bool selected_exchange(
    Nfc* nfc,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    if(!poller) return false;

    SelectedSession s{};
    s.thread_id = furi_thread_get_current_id();
    s.tx = bit_buffer_alloc(64);
    s.rx = bit_buffer_alloc(64);
    s.command = command;
    s.command_len = command_len;
    s.response = response;
    s.response_capacity = response_capacity;
    if(!s.tx || !s.rx) {
        if(s.tx) bit_buffer_free(s.tx);
        if(s.rx) bit_buffer_free(s.rx);
        nfc_poller_free(poller);
        return false;
    }

    nfc_poller_start(poller, selected_callback, &s);
    const uint32_t flags =
        furi_thread_flags_wait(WriterDoneFlag, FuriFlagWaitAny, FuriWaitForever);
    if(flags & WriterDoneFlag) furi_thread_flags_clear(WriterDoneFlag);
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    bit_buffer_free(s.tx);
    bit_buffer_free(s.rx);
    if(response_len) *response_len = s.response_len;
    return s.ok;
}

bool gen4_command(
    Nfc* nfc,
    const uint8_t password[4],
    uint8_t opcode,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    uint8_t command[64];
    if(payload_len + 6U > sizeof(command)) return false;
    size_t len = 0;
    command[len++] = 0xCF;
    memcpy(command + len, password, 4);
    len += 4;
    command[len++] = opcode;
    if(payload_len) {
        memcpy(command + len, payload, payload_len);
        len += payload_len;
    }
    return selected_exchange(nfc, command, len, response, response_capacity, response_len);
}

bool gen4_detect(Nfc* nfc) {
    static const uint8_t pwd[4] = {0, 0, 0, 0};
    uint8_t response[8]{};
    size_t response_len = 0;
    return gen4_command(nfc, pwd, 0xCC, nullptr, 0, response, sizeof(response), &response_len) &&
           response_len == 5U;
}

bool gen4_write_block(Nfc* nfc, const uint8_t password[4], uint8_t block, const uint8_t data[16]) {
    uint8_t payload[17];
    payload[0] = block;
    memcpy(payload + 1, data, 16);
    uint8_t response[4]{};
    size_t response_len = 0;
    return gen4_command(
               nfc,
               password,
               0xCD,
               payload,
               sizeof(payload),
               response,
               sizeof(response),
               &response_len) &&
           response_len == 2U;
}

bool gen4_write_dump(Nfc* nfc, const uint8_t password[4], const BrTagDump& dump) {
    for(uint8_t block = 1; block < BR_BLOCK_COUNT; ++block) {
        if(!gen4_write_block(nfc, password, block, dump.blocks[block])) return false;
    }
    return gen4_write_block(nfc, password, 0, dump.blocks[0]);
}

bool gen4_read_config(Nfc* nfc, const uint8_t password[4], uint8_t config[32]) {
    size_t response_len = 0;
    return gen4_command(nfc, password, 0xC6, nullptr, 0, config, 32, &response_len) &&
           response_len == 32U;
}

bool gen4_fuse_config(Nfc* nfc, const uint8_t password[4], const uint8_t config[32]) {
    uint8_t response[4]{};
    size_t response_len = 0;
    // F1 consumes the 30 configuration bytes; the final two bytes returned by C6 are CRC.
    return gen4_command(
               nfc, password, 0xF1, config, 30, response, sizeof(response), &response_len) &&
           response_len == 2U;
}

bool gen3_read_block0(Nfc* nfc, uint8_t block0[16]) {
    const uint8_t command[2] = {0x30, 0x00};
    uint8_t response[20]{};
    size_t response_len = 0;
    if(!selected_exchange(
           nfc, command, sizeof(command), response, sizeof(response), &response_len) ||
       response_len != BR_BLOCK_SIZE)
        return false;
    memcpy(block0, response, BR_BLOCK_SIZE);
    return true;
}

bool gen3_write_block0(Nfc* nfc, const uint8_t block0[16]) {
    uint8_t command[21] = {0x90, 0xF0, 0xCC, 0xCC, 0x10};
    memcpy(command + 5, block0, 16);
    uint8_t response[8]{};
    size_t response_len = 0;
    return selected_exchange(
               nfc, command, sizeof(command), response, sizeof(response), &response_len) &&
           response_len > 0U;
}

bool gen3_change_uid(Nfc* nfc, const uint8_t uid[4]) {
    // Proxmark3 keeps the APDU length byte at 0x07 even for a 4-byte UID and
    // transmits only uidlen bytes. This command changes the anticollision UID
    // independently of block 0, making it the correct lock-verification probe.
    uint8_t command[9] = {0x90, 0xFB, 0xCC, 0xCC, 0x07};
    memcpy(command + 5, uid, 4);
    uint8_t response[8]{};
    size_t response_len = 0;
    return selected_exchange(
               nfc, command, sizeof(command), response, sizeof(response), &response_len) &&
           response_len > 0U;
}

bool gen3_freeze(Nfc* nfc) {
    const uint8_t command[5] = {0x90, 0xFD, 0x11, 0x11, 0x00};
    uint8_t response[8]{};
    size_t response_len = 0;
    return selected_exchange(
               nfc, command, sizeof(command), response, sizeof(response), &response_len) &&
           response_len > 0U;
}

bool read_identity(Nfc* nfc, uint8_t uid[4], uint8_t atqa[2], uint8_t* sak) {
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    if(!poller) return false;
    const bool found = nfc_poller_detect(poller);
    bool ok = false;
    if(found) {
        const Iso14443_3aData* data =
            static_cast<const Iso14443_3aData*>(nfc_poller_get_data(poller));
        if(data && data->uid_len == 4U) {
            if(uid) memcpy(uid, data->uid, 4);
            if(atqa) memcpy(atqa, data->atqa, 2);
            if(sak) *sak = data->sak;
            ok = true;
        }
    }
    nfc_poller_free(poller);
    return ok;
}

bool wait_uid(Nfc* nfc, uint8_t uid[4]) {
    return read_identity(nfc, uid, nullptr, nullptr);
}

void key_from_bytes(const uint8_t bytes[6], MfClassicKey* key) {
    memcpy(key->data, bytes, 6);
}

void dump_key_a(const BrTagDump& dump, uint8_t sector, MfClassicKey* key) {
    const uint8_t trailer = static_cast<uint8_t>(sector * 4U + 3U);
    key_from_bytes(dump.blocks[trailer], key);
}

void dump_key_b(const BrTagDump& dump, uint8_t sector, MfClassicKey* key) {
    const uint8_t trailer = static_cast<uint8_t>(sector * 4U + 3U);
    key_from_bytes(dump.blocks[trailer] + 10, key);
}

bool write_with_either_key(
    Nfc* nfc,
    uint8_t block,
    const MfClassicBlock* data,
    MfClassicKey key_a,
    MfClassicKey key_b,
    bool prefer_b) {
    MfClassicKey* first = prefer_b ? &key_b : &key_a;
    MfClassicKey* second = prefer_b ? &key_a : &key_b;
    MfClassicKeyType first_type = prefer_b ? MfClassicKeyTypeB : MfClassicKeyTypeA;
    MfClassicKeyType second_type = prefer_b ? MfClassicKeyTypeA : MfClassicKeyTypeB;

    MfClassicBlock copy = *data;
    if(mf_classic_poller_sync_write_block(nfc, block, first, first_type, &copy) ==
       MfClassicErrorNone)
        return true;
    copy = *data;
    return mf_classic_poller_sync_write_block(nfc, block, second, second_type, &copy) ==
           MfClassicErrorNone;
}

bool relax_sector_access(Nfc* nfc, const BrTagDump& current, uint8_t sector) {
    const uint8_t trailer_num = static_cast<uint8_t>(sector * 4U + 3U);
    MfClassicBlock relaxed{};
    memcpy(relaxed.data, current.blocks[trailer_num], BR_BLOCK_SIZE);
    relaxed.data[6] = 0xFF;
    relaxed.data[7] = 0x07;
    relaxed.data[8] = 0x80;
    relaxed.data[9] = 0x69;

    MfClassicKey key_a{};
    MfClassicKey key_b{};
    dump_key_a(current, sector, &key_a);
    dump_key_b(current, sector, &key_b);

    // Bambu trailers allow the access-condition bytes to be rewritten with Key B.
    // Try B first, then A for magic-card variants with looser trailer semantics.
    return write_with_either_key(nfc, trailer_num, &relaxed, key_a, key_b, true);
}

bool restore_sector_trailer(Nfc* nfc, const BrTagDump& current, uint8_t sector) {
    const uint8_t trailer_num = static_cast<uint8_t>(sector * 4U + 3U);
    MfClassicBlock trailer{};
    memcpy(trailer.data, current.blocks[trailer_num], BR_BLOCK_SIZE);
    MfClassicKey key_a{};
    MfClassicKey key_b{};
    dump_key_a(current, sector, &key_a);
    dump_key_b(current, sector, &key_b);
    // Once FF0780 access is active, Key A should be sufficient. Keep a B fallback
    // because clone implementations vary in how they apply trailer permissions.
    return write_with_either_key(nfc, trailer_num, &trailer, key_a, key_b, false);
}

bool gen2_write_block0_preserving_acl(
    Nfc* nfc,
    const BrTagDump& current,
    const uint8_t block0[BR_BLOCK_SIZE]) {
    if(!relax_sector_access(nfc, current, 0)) return false;

    MfClassicKey key_a{};
    MfClassicKey key_b{};
    dump_key_a(current, 0, &key_a);
    dump_key_b(current, 0, &key_b);
    MfClassicBlock out{};
    memcpy(out.data, block0, BR_BLOCK_SIZE);

    const bool wrote = write_with_either_key(nfc, 0, &out, key_a, key_b, false);
    // Never leave sector 0 in transport access merely because the UID probe/write
    // failed. Restoring the selected dump's trailer is part of the operation.
    const bool restored_acl = restore_sector_trailer(nfc, current, 0);
    return wrote && restored_acl;
}

bool rewrite_dump_from_current(
    Nfc* nfc,
    const BrTagDump& target,
    const BrTagDump& current,
    bool write_block0_standard) {
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        const uint8_t first = static_cast<uint8_t>(sector * 4U);
        const uint8_t trailer_num = static_cast<uint8_t>(first + 3U);
        if(!relax_sector_access(nfc, current, sector)) return false;

        MfClassicKey key_a{};
        MfClassicKey key_b{};
        dump_key_a(current, sector, &key_a);
        dump_key_b(current, sector, &key_b);

        for(uint8_t block = first; block < trailer_num; ++block) {
            if(block == 0U && !write_block0_standard) continue;
            MfClassicBlock out{};
            memcpy(out.data, target.blocks[block], BR_BLOCK_SIZE);
            if(!write_with_either_key(nfc, block, &out, key_a, key_b, false)) {
                // Best effort: restore the current trailer before aborting so a
                // failed blanking operation does not leave an unlocked ACL behind.
                restore_sector_trailer(nfc, current, sector);
                return false;
            }
        }

        MfClassicBlock target_trailer{};
        memcpy(target_trailer.data, target.blocks[trailer_num], BR_BLOCK_SIZE);
        if(!write_with_either_key(nfc, trailer_num, &target_trailer, key_a, key_b, false)) {
            restore_sector_trailer(nfc, current, sector);
            return false;
        }
    }
    return true;
}

bool verify_dump_with_keys(Nfc* nfc, const BrTagDump& expected, const MfClassicDeviceKeys& keys) {
    MfClassicData* data = mf_classic_alloc();
    if(!data) return false;
    const MfClassicError error = mf_classic_poller_sync_read(nfc, &keys, data);
    bool ok = error == MfClassicErrorNone || error == MfClassicErrorPartialRead;
    if(ok) {
        for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
            if(!mf_classic_is_block_read(data, block) ||
               memcmp(data->block[block].data, expected.blocks[block], BR_BLOCK_SIZE) != 0) {
                ok = false;
                break;
            }
        }
    }
    mf_classic_free(data);
    return ok;
}

void make_dump_keys(const BrTagDump& dump, MfClassicDeviceKeys* keys) {
    memset(keys, 0, sizeof(*keys));
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        dump_key_a(dump, sector, &keys->key_a[sector]);
        dump_key_b(dump, sector, &keys->key_b[sector]);
        FURI_BIT_SET(keys->key_a_mask, sector);
        FURI_BIT_SET(keys->key_b_mask, sector);
    }
}

bool verify_dump(Nfc* nfc, const BrTagDump& expected) {
    MfClassicDeviceKeys keys{};
    make_dump_keys(expected, &keys);
    return verify_dump_with_keys(nfc, expected, keys);
}

void make_default_keys(MfClassicDeviceKeys* keys) {
    memset(keys, 0, sizeof(*keys));
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        memcpy(keys->key_a[sector].data, DefaultKeyBytes, MF_CLASSIC_KEY_SIZE);
        memcpy(keys->key_b[sector].data, DefaultKeyBytes, MF_CLASSIC_KEY_SIZE);
        FURI_BIT_SET(keys->key_a_mask, sector);
        FURI_BIT_SET(keys->key_b_mask, sector);
    }
}

bool verify_blank_dump(Nfc* nfc, const BrTagDump& blank) {
    uint8_t live_uid[4]{};
    if(!wait_uid(nfc, live_uid) || memcmp(live_uid, blank.blocks[0], 4) != 0) return false;
    MfClassicDeviceKeys keys{};
    make_default_keys(&keys);
    return verify_dump_with_keys(nfc, blank, keys);
}

bool verify_block0_exact(Nfc* nfc, const BrTagDump& expected) {
    MfClassicKey key{};
    dump_key_a(expected, 0, &key);
    MfClassicBlock block{};
    if(mf_classic_poller_sync_read_block(nfc, 0, &key, MfClassicKeyTypeA, &block) !=
       MfClassicErrorNone)
        return false;
    return memcmp(block.data, expected.blocks[0], BR_BLOCK_SIZE) == 0;
}

void make_probe_block0(const uint8_t source[16], uint8_t probe[16], uint8_t probe_uid[4]) {
    memcpy(probe, source, 16);
    do {
        furi_hal_random_fill_buf(probe_uid, 4);
    } while(memcmp(probe_uid, source, 4) == 0 || (probe_uid[0] == 0U && probe_uid[1] == 0U &&
                                                  probe_uid[2] == 0U && probe_uid[3] == 0U));
    memcpy(probe, probe_uid, 4);
    probe[4] = probe_uid[0] ^ probe_uid[1] ^ probe_uid[2] ^ probe_uid[3];
}

void make_blank_dump(BrTagDump* blank) {
    // Canonical empty magic MIFARE Classic 1K image used by Flipper NFC Magic:
    // UID 00 01 02 03, valid BCC 00, SAK 08, ATQA 04 00, zero data blocks,
    // and transport/default keys + access bytes in every sector trailer.
    memset(blank, 0, sizeof(*blank));
    static const uint8_t FactoryBlock0[BR_BLOCK_SIZE] = {
        0x00,
        0x01,
        0x02,
        0x03,
        0x00,
        0x08,
        0x04,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    memcpy(blank->blocks[0], FactoryBlock0, sizeof(FactoryBlock0));
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        uint8_t* trailer = blank->blocks[sector * 4U + 3U];
        memset(trailer, 0xFF, 6);
        trailer[6] = 0xFF;
        trailer[7] = 0x07;
        trailer[8] = 0x80;
        trailer[9] = 0x69;
        memset(trailer + 10, 0xFF, 6);
    }
    for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block)
        blank->read[block] = true;
}

bool is_ufuid_config(const uint8_t config[16]) {
    return config[0] == 0x7A && config[1] == 0xFF && config[2] == 0x00 && config[8] == 0xBA &&
           config[9] == 0xFA && config[15] == 0x08;
}

bool restore_source(Nfc* nfc, BrMagicFamily family, const BrTagDump& source) {
    if(family == BrMagicFamily::Gen2) {
        return gen2_write_block0_preserving_acl(nfc, source, source.blocks[0]);
    }
    if(family == BrMagicFamily::Gen3) return gen3_change_uid(nfc, source.blocks[0]);
    if(family == BrMagicFamily::Gen4) {
        static const uint8_t pwd[4] = {0, 0, 0, 0};
        if(gen4_write_block(nfc, pwd, 0, source.blocks[0])) return true;
        // After a fused password change the default backdoor is intentionally gone.
        // If direct block-0 writing nevertheless remained enabled, use that same
        // path to restore the Bambu UID before declaring the lock attempt failed.
        return gen2_write_block0_preserving_acl(nfc, source, source.blocks[0]);
    }
    if(family == BrMagicFamily::Gen1Alt)
        return run_gen1(nfc, 0x20, 0x23, nullptr, source.blocks[0], nullptr, nullptr, false);
    if(family == BrMagicFamily::Gen1B)
        return run_gen1(nfc, 0x40, 0x00, nullptr, source.blocks[0], nullptr, nullptr, false);
    if(family == BrMagicFamily::Gen1A || family == BrMagicFamily::Ufuid)
        return run_gen1(nfc, 0x40, 0x43, nullptr, source.blocks[0], nullptr, nullptr, false);
    return false;
}

enum class ProbeOutcome : uint8_t {
    Changed,
    Blocked,
    Unsafe,
};

bool write_probe_block0(
    Nfc* nfc,
    BrMagicFamily family,
    const BrTagDump& source,
    const uint8_t probe_block0[16]) {
    if(family == BrMagicFamily::Gen4) {
        static const uint8_t pwd[4] = {0, 0, 0, 0};
        if(gen4_write_block(nfc, pwd, 0, probe_block0)) return true;
        // A fused/randomized Gen4 must also reject ordinary DirectWrite/CUID
        // block-0 writes. Testing both paths prevents a false lock result.
        return gen2_write_block0_preserving_acl(nfc, source, probe_block0);
    }
    if(family == BrMagicFamily::Gen1Alt)
        return run_gen1(nfc, 0x20, 0x23, nullptr, probe_block0, nullptr, nullptr, false);
    if(family == BrMagicFamily::Gen1B)
        return run_gen1(nfc, 0x40, 0x00, nullptr, probe_block0, nullptr, nullptr, false);
    if(family == BrMagicFamily::Gen1A || family == BrMagicFamily::Ufuid)
        return run_gen1(nfc, 0x40, 0x43, nullptr, probe_block0, nullptr, nullptr, false);
    if(family == BrMagicFamily::Gen2)
        return gen2_write_block0_preserving_acl(nfc, source, probe_block0);
    if(family == BrMagicFamily::Gen3) return gen3_change_uid(nfc, probe_block0);
    return false;
}

bool safe_to_lock(Nfc* nfc, const BrTagDump& source);

ProbeOutcome probe_uid_mutability(
    Nfc* nfc,
    BrMagicFamily family,
    const BrTagDump& source,
    const uint8_t probe_block0[16],
    const uint8_t probe_uid[4]) {
    // A single RF timeout is not evidence of a lock. Require three cleanly blocked
    // attempts, with the original Bambu image still exact between attempts.
    for(uint8_t attempt = 0; attempt < 3U; ++attempt) {
        if(!safe_to_lock(nfc, source)) return ProbeOutcome::Unsafe;

        const bool write_ok = write_probe_block0(nfc, family, source, probe_block0);
        furi_delay_ms(35);

        uint8_t observed[4]{};
        if(wait_uid(nfc, observed)) {
            if(memcmp(observed, probe_uid, 4) == 0) return ProbeOutcome::Changed;
            if(memcmp(observed, source.blocks[0], 4) != 0) return ProbeOutcome::Unsafe;
        }

        // If the command claimed success but the UID did not change, or if the
        // transaction failed, only count it as a blocked attempt when the entire
        // selected Bambu image is still exact. Otherwise page 0 may be in an
        // indeterminate state and must be restored by the caller.
        if(!safe_to_lock(nfc, source)) return ProbeOutcome::Unsafe;
        if(write_ok) furi_delay_ms(25);
    }
    return ProbeOutcome::Blocked;
}

bool safe_to_lock(Nfc* nfc, const BrTagDump& source) {
    // This is intentionally redundant. Gen3 can alter the anticollision UID without
    // altering block 0, so all three conditions are mandatory before any irreversible
    // operation: live UID, complete dump, and all 16 bytes of the manufacturer block.
    uint8_t live_uid[4]{};
    uint8_t live_atqa[2]{};
    uint8_t live_sak = 0;
    if(!read_identity(nfc, live_uid, live_atqa, &live_sak)) return false;
    if(memcmp(live_uid, source.blocks[0], 4) != 0) return false;
    if(live_sak != source.blocks[0][5]) return false;
    if(live_atqa[0] != source.blocks[0][6] || live_atqa[1] != source.blocks[0][7]) return false;
    return verify_dump(nfc, source) && verify_block0_exact(nfc, source);
}

bool lock_ufuid(Nfc* nfc, const BrTagDump& source) {
    if(!safe_to_lock(nfc, source)) return false;
    return run_gen1(nfc, 0x40, 0x43, nullptr, nullptr, nullptr, UfuidSealConfig, false);
}

bool lock_gen3(Nfc* nfc, const BrTagDump& source) {
    if(!safe_to_lock(nfc, source)) return false;
    return gen3_freeze(nfc);
}

bool lock_gen4(Nfc* nfc, const BrTagDump& source) {
    if(!safe_to_lock(nfc, source)) return false;
    static const uint8_t default_pwd[4] = {0, 0, 0, 0};
    uint8_t config[32]{};
    if(!gen4_read_config(nfc, default_pwd, config)) return false;

    uint8_t random_pwd[4];
    do {
        furi_hal_random_fill_buf(random_pwd, sizeof(random_pwd));
    } while((random_pwd[0] | random_pwd[1] | random_pwd[2] | random_pwd[3]) == 0U);

    // Preserve the card's existing configuration wherever possible, but force the
    // settings needed for a Bambu-like MIFARE Classic 1K personality and disable
    // ordinary direct block-0 writes. F1 permanently fuses these configuration
    // bytes. The random backdoor password is deliberately never persisted.
    config[0] = 0x00; // MIFARE Classic protocol
    config[1] = 0x00; // 4-byte UID
    memcpy(config + 2, random_pwd, 4);
    config[6] = 0x02; // shadow/GTU disabled
    config[24] = source.blocks[0][6];
    config[25] = source.blocks[0][7];
    config[26] = source.blocks[0][5];
    config[27] = 0x00;
    config[29] = 0x01; // direct block-0 write disabled
    return gen4_fuse_config(nfc, default_pwd, config);
}

bool blank_after_unlocked(
    Nfc* nfc,
    BrMagicFamily family,
    const BrTagDump& source,
    BrWriteContext* ctx) {
    BrTagDump blank{};
    make_blank_dump(&blank);
    report(ctx, "No permanent lock; blanking tag safely...");

    bool wrote = false;
    if(family == BrMagicFamily::Gen2) {
        wrote = rewrite_dump_from_current(nfc, blank, source, true);
    } else if(family == BrMagicFamily::Gen3) {
        wrote = rewrite_dump_from_current(nfc, blank, source, false) &&
                gen3_write_block0(nfc, blank.blocks[0]);
    } else if(family == BrMagicFamily::Gen1Alt) {
        wrote = run_gen1(nfc, 0x20, 0x23, &blank, nullptr, nullptr, nullptr, false);
    } else if(family == BrMagicFamily::Gen1B) {
        wrote = run_gen1(nfc, 0x40, 0x00, &blank, nullptr, nullptr, nullptr, false);
    } else if(family == BrMagicFamily::Gen1A || family == BrMagicFamily::Ufuid) {
        wrote = run_gen1(nfc, 0x40, 0x43, &blank, nullptr, nullptr, nullptr, false);
    } else if(family == BrMagicFamily::Gen4) {
        static const uint8_t default_pwd[4] = {0, 0, 0, 0};
        wrote = gen4_write_dump(nfc, default_pwd, blank);
    }
    if(!wrote) return false;
    furi_delay_ms(40);
    return verify_blank_dump(nfc, blank);
}

bool read_bambu_current_dump(
    Nfc* nfc,
    const uint8_t uid[BR_UID_SIZE],
    BrTagDump* dump,
    MfClassicDeviceKeys* derived_keys) {
    if(!nfc || !uid || !dump || !derived_keys) return false;
    if(!br_derive_sector_keys(uid, derived_keys)) return false;

    MfClassicData* data = mf_classic_alloc();
    if(!data) return false;
    const MfClassicError error = mf_classic_poller_sync_read(nfc, derived_keys, data);
    bool ok = error == MfClassicErrorNone || error == MfClassicErrorPartialRead;
    memset(dump, 0, sizeof(*dump));
    if(ok) {
        for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
            if(!mf_classic_is_block_read(data, block)) {
                ok = false;
                break;
            }
            memcpy(dump->blocks[block], data->block[block].data, BR_BLOCK_SIZE);
            dump->read[block] = true;
        }
    }
    mf_classic_free(data);
    if(!ok) return false;

    // MIFARE reads do not always expose trailer key bytes. We know the exact
    // current Bambu A/B keys from the live UID, so place them explicitly into
    // the in-memory current image used by the wipe/ACL transition code.
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        const uint8_t trailer = static_cast<uint8_t>(sector * 4U + 3U);
        memcpy(dump->blocks[trailer], derived_keys->key_a[sector].data, MF_CLASSIC_KEY_SIZE);
        memcpy(dump->blocks[trailer] + 10, derived_keys->key_b[sector].data, MF_CLASSIC_KEY_SIZE);
    }
    return true;
}

BrMagicFamily detect_magic_family_for_wipe(Nfc* nfc) {
    if(gen4_detect(nfc)) return BrMagicFamily::Gen4;

    if(run_gen1(nfc, 0x40, 0x43, nullptr, nullptr, nullptr, nullptr, true)) {
        bool is_mf4 = false;
        if(detect_mf4_overlay(nfc, &is_mf4) && is_mf4) return BrMagicFamily::Mf4Unsupported;
        uint8_t config[16]{};
        if(run_gen1(nfc, 0x40, 0x43, nullptr, nullptr, config, nullptr, false) &&
           is_ufuid_config(config))
            return BrMagicFamily::Ufuid;
        return BrMagicFamily::Gen1A;
    }
    if(run_gen1(nfc, 0x40, 0x00, nullptr, nullptr, nullptr, nullptr, true))
        return BrMagicFamily::Gen1B;
    if(run_gen1(nfc, 0x20, 0x23, nullptr, nullptr, nullptr, nullptr, true))
        return BrMagicFamily::Gen1Alt;

    uint8_t block0[BR_BLOCK_SIZE]{};
    if(gen3_read_block0(nfc, block0)) return BrMagicFamily::Gen3;

    // DirectWrite/Gen2 has no universally reliable read-only signature. At this
    // point the tag has already been fully read with its UID-derived Bambu keys;
    // the wipe performs a canonical block-0 write first and aborts before erasing
    // other blocks if that DirectWrite operation is unavailable.
    return BrMagicFamily::Gen2;
}

bool gen4_write_blocks_1_to_63(Nfc* nfc, const uint8_t password[4], const BrTagDump& dump) {
    for(uint8_t block = 1; block < BR_BLOCK_COUNT; ++block) {
        if(!gen4_write_block(nfc, password, block, dump.blocks[block])) return false;
    }
    return true;
}

bool write_canonical_blank(
    Nfc* nfc,
    BrMagicFamily family,
    const BrTagDump& current,
    const BrTagDump& blank) {
    const uint8_t* blank_uid = blank.blocks[0];
    bool block0_ready = false;

    if(family == BrMagicFamily::Gen4) {
        static const uint8_t default_pwd[4] = {0, 0, 0, 0};
        block0_ready = gen4_write_block(nfc, default_pwd, 0, blank.blocks[0]);
    } else if(family == BrMagicFamily::Gen3) {
        if(gen3_change_uid(nfc, blank_uid)) {
            furi_delay_ms(30);
            uint8_t live_uid[4]{};
            if(wait_uid(nfc, live_uid) && memcmp(live_uid, blank_uid, 4) == 0) {
                block0_ready = gen3_write_block0(nfc, blank.blocks[0]);
                if(!block0_ready) {
                    // If page/block-0 write is unavailable, restore the original
                    // live UID before returning. No other blocks have been erased.
                    gen3_change_uid(nfc, current.blocks[0]);
                }
            }
        }
    } else if(family == BrMagicFamily::Gen2) {
        block0_ready = gen2_write_block0_preserving_acl(nfc, current, blank.blocks[0]);
    } else if(family == BrMagicFamily::Gen1Alt) {
        block0_ready =
            run_gen1(nfc, 0x20, 0x23, nullptr, blank.blocks[0], nullptr, nullptr, false);
    } else if(family == BrMagicFamily::Gen1B) {
        block0_ready =
            run_gen1(nfc, 0x40, 0x00, nullptr, blank.blocks[0], nullptr, nullptr, false);
    } else if(family == BrMagicFamily::Gen1A || family == BrMagicFamily::Ufuid) {
        block0_ready =
            run_gen1(nfc, 0x40, 0x43, nullptr, blank.blocks[0], nullptr, nullptr, false);
    }
    if(!block0_ready) return false;

    furi_delay_ms(35);
    uint8_t live_uid[4]{};
    if(!wait_uid(nfc, live_uid) || memcmp(live_uid, blank_uid, 4) != 0) return false;

    if(family == BrMagicFamily::Gen4) {
        static const uint8_t default_pwd[4] = {0, 0, 0, 0};
        return gen4_write_blocks_1_to_63(nfc, default_pwd, blank);
    }
    if(family == BrMagicFamily::Gen3 || family == BrMagicFamily::Gen2)
        return rewrite_dump_from_current(nfc, blank, current, false);
    if(family == BrMagicFamily::Gen1Alt)
        return run_gen1(nfc, 0x20, 0x23, &blank, nullptr, nullptr, nullptr, false, true);
    if(family == BrMagicFamily::Gen1B)
        return run_gen1(nfc, 0x40, 0x00, &blank, nullptr, nullptr, nullptr, false, true);
    if(family == BrMagicFamily::Gen1A || family == BrMagicFamily::Ufuid)
        return run_gen1(nfc, 0x40, 0x43, &blank, nullptr, nullptr, nullptr, false, true);
    return false;
}

bool initial_write(
    Nfc* nfc,
    const BrTagDump& source,
    BrMagicFamily* family,
    uint8_t gen1_config[16],
    bool* have_gen1_config) {
    static const uint8_t default_pwd[4] = {0, 0, 0, 0};
    *have_gen1_config = false;

    if(gen4_detect(nfc)) {
        *family = BrMagicFamily::Gen4;
        return gen4_write_dump(nfc, default_pwd, source);
    }

    if(run_gen1(nfc, 0x40, 0x43, nullptr, nullptr, nullptr, nullptr, true)) {
        *family = BrMagicFamily::Gen1A;

        // MF4 mirrors hidden personality/configuration bits into sector-trailer
        // bytes. Exact Bambu trailers would alter its live SAK/ATQA and then the
        // UID write can permanently remove the backdoor. Detect that overlay by
        // comparing a backdoor trailer read with a normal authenticated read, and
        // reject it before touching any user data.
        bool is_mf4 = false;
        if(!detect_mf4_overlay(nfc, &is_mf4)) return false;
        if(is_mf4) {
            *family = BrMagicFamily::Mf4Unsupported;
            return false;
        }

        if(run_gen1(nfc, 0x40, 0x43, nullptr, nullptr, gen1_config, nullptr, false)) {
            *have_gen1_config = true;
            if(is_ufuid_config(gen1_config)) *family = BrMagicFamily::Ufuid;
        }
        return run_gen1(nfc, 0x40, 0x43, &source, nullptr, nullptr, nullptr, false);
    }

    // Gen1B acknowledges the same 7-bit 0x40 wakeup but skips 0x43. Test it
    // only after a real Gen1A 40/43 sequence has failed.
    if(run_gen1(nfc, 0x40, 0x00, nullptr, nullptr, nullptr, nullptr, true)) {
        *family = BrMagicFamily::Gen1B;
        return run_gen1(nfc, 0x40, 0x00, &source, nullptr, nullptr, nullptr, false);
    }

    if(run_gen1(nfc, 0x20, 0x23, nullptr, nullptr, nullptr, nullptr, true)) {
        *family = BrMagicFamily::Gen1Alt;
        if(run_gen1(nfc, 0x20, 0x23, nullptr, nullptr, gen1_config, nullptr, false))
            *have_gen1_config = true;
        return run_gen1(nfc, 0x20, 0x23, &source, nullptr, nullptr, nullptr, false);
    }

    // Unknown non-Gen1/Gen4 target. NEVER use a random UID for preflight here:
    // DirectWrite OTP tags can make the first block-0 write permanent. Detect Gen3
    // read-only, otherwise the first manufacturer-block write is the selected Bambu
    // block 0 itself. This guarantees a write-once target can only seal the intended UID.
    uint8_t gen3_original0[BR_BLOCK_SIZE]{};
    const bool is_gen3 = gen3_read_block0(nfc, gen3_original0);
    bool is_gen2 = false;

    MfClassicKey default_key{};
    key_from_bytes(DefaultKeyBytes, &default_key);

    if(is_gen3) {
        // Some Gen3 variants support UID-only changes but not full block-0 writes.
        // Prove full-page support using the exact page already on the blank tag; no
        // temporary UID or manufacturer data is introduced by this capability test.
        if(!gen3_write_block0(nfc, gen3_original0)) return false;
        uint8_t checked0[BR_BLOCK_SIZE]{};
        if(!gen3_read_block0(nfc, checked0) ||
           memcmp(checked0, gen3_original0, BR_BLOCK_SIZE) != 0)
            return false;
    } else {
        // A normal MIFARE card will reject block 0. A DirectWrite/Gen2/OTP card may
        // accept it; therefore the ONLY value ever attempted as its first block-0
        // write is the final selected Bambu manufacturer block.
        MfClassicBlock source0{};
        memcpy(source0.data, source.blocks[0], BR_BLOCK_SIZE);
        if(mf_classic_poller_sync_write_block(nfc, 0, &default_key, MfClassicKeyTypeA, &source0) !=
           MfClassicErrorNone)
            return false;
        furi_delay_ms(35);

        uint8_t live_uid[4]{};
        if(!wait_uid(nfc, live_uid) || memcmp(live_uid, source.blocks[0], 4) != 0) return false;
        MfClassicBlock checked{};
        if(mf_classic_poller_sync_read_block(nfc, 0, &default_key, MfClassicKeyTypeA, &checked) !=
               MfClassicErrorNone ||
           memcmp(checked.data, source.blocks[0], BR_BLOCK_SIZE) != 0)
            return false;
        is_gen2 = true;
    }

    // Program data before each trailer. For Gen2/DirectWrite/OTP, the correct
    // Bambu block 0 is already in place before any restrictive Bambu ACL is
    // installed. Gen3 gets the selected manufacturer block again as the final step.
    for(uint8_t block = 1; block <= 2; ++block) {
        MfClassicBlock out{};
        memcpy(out.data, source.blocks[block], BR_BLOCK_SIZE);
        if(mf_classic_poller_sync_write_block(nfc, block, &default_key, MfClassicKeyTypeA, &out) !=
           MfClassicErrorNone)
            return false;
    }

    MfClassicBlock trailer0{};
    memcpy(trailer0.data, source.blocks[3], BR_BLOCK_SIZE);
    if(mf_classic_poller_sync_write_block(nfc, 3, &default_key, MfClassicKeyTypeA, &trailer0) !=
       MfClassicErrorNone)
        return false;

    for(uint8_t sector = 1; sector < BR_SECTOR_COUNT; ++sector) {
        const uint8_t first = static_cast<uint8_t>(sector * 4U);
        const uint8_t trailer = static_cast<uint8_t>(first + 3U);
        for(uint8_t block = first; block < trailer; ++block) {
            MfClassicBlock out{};
            memcpy(out.data, source.blocks[block], BR_BLOCK_SIZE);
            if(mf_classic_poller_sync_write_block(
                   nfc, block, &default_key, MfClassicKeyTypeA, &out) != MfClassicErrorNone)
                return false;
        }
        MfClassicBlock tr{};
        memcpy(tr.data, source.blocks[trailer], BR_BLOCK_SIZE);
        if(mf_classic_poller_sync_write_block(
               nfc, trailer, &default_key, MfClassicKeyTypeA, &tr) != MfClassicErrorNone)
            return false;
    }

    if(is_gen2) {
        *family = BrMagicFamily::Gen2;
        return true;
    }
    if(gen3_write_block0(nfc, source.blocks[0])) {
        *family = BrMagicFamily::Gen3;
        return true;
    }
    return false;
}
} // namespace

const char* br_magic_family_name(BrMagicFamily family) {
    switch(family) {
    case BrMagicFamily::Gen1A:
        return "Gen1A / UID";
    case BrMagicFamily::Gen1B:
        return "Gen1B";
    case BrMagicFamily::Gen1Alt:
        return "FUID / alt Gen1";
    case BrMagicFamily::Mf4Unsupported:
        return "MF4 (not safe for exact clone)";
    case BrMagicFamily::Ufuid:
        return "UFUID";
    case BrMagicFamily::Gen2:
        return "Gen2 / CUID";
    case BrMagicFamily::Gen3:
        return "Gen3 / APDU";
    case BrMagicFamily::Gen4:
        return "Gen4 / Ultimate";
    default:
        return "Unknown magic";
    }
}

int32_t br_write_worker(void* context) {
    BrWriteContext* ctx = static_cast<BrWriteContext*>(context);
    BrWriteResult* result = ctx ? ctx->result : nullptr;
    if(!ctx || !result) return 0;
    memset(result, 0, sizeof(*result));
    result->family = BrMagicFamily::Unknown;

    BrTagDump source{};
    BrTagInfo source_info{};
    if(!br_load_tag_dump(ctx->storage, ctx->source_path, &source, &source_info) ||
       !source_info.complete) {
        report(ctx, "Selected dump is incomplete or invalid");
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }
    for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
        if(!source.read[block]) {
            report(ctx, "Selected dump is missing blocks");
            if(ctx->dispatcher)
                view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
            return 0;
        }
    }
    const uint8_t expected_bcc = source.blocks[0][0] ^ source.blocks[0][1] ^ source.blocks[0][2] ^
                                 source.blocks[0][3];
    if(source.blocks[0][4] != expected_bcc) {
        report(ctx, "Selected dump has an invalid block-0 BCC");
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        report(ctx, "Could not acquire NFC hardware");
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "Hold blank magic MIFARE 1K tag steady");
    uint8_t target_uid[4]{};
    while(!cancelled(ctx) && !wait_uid(nfc, target_uid))
        furi_delay_ms(120);
    if(cancelled(ctx)) {
        nfc_free(nfc);
        return 0;
    }

    MfClassicType target_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(nfc, &target_type) != MfClassicErrorNone ||
       target_type != MfClassicType1k) {
        report(ctx, "Target is not a MIFARE Classic 1K tag");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "Writing Bambu dump; block 0 is last...");
    BrMagicFamily family = BrMagicFamily::Unknown;
    uint8_t gen1_config[16]{};
    bool have_gen1_config = false;
    if(!initial_write(nfc, source, &family, gen1_config, &have_gen1_config)) {
        result->family = family;
        snprintf(
            result->family_name, sizeof(result->family_name), "%s", br_magic_family_name(family));
        if(family == BrMagicFamily::Mf4Unsupported)
            report(ctx, "MF4 rejected: exact Bambu trailers would alter live identity");
        else
            report(ctx, "Could not program this magic-tag type");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    result->family = family;
    snprintf(result->family_name, sizeof(result->family_name), "%s", br_magic_family_name(family));

    report(ctx, "Verifying exact Bambu dump before any lock...");
    if(!safe_to_lock(nfc, source)) {
        report(ctx, "Verification failed; no lock command sent");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    uint8_t probe_block0[16]{};
    uint8_t probe_uid[4]{};
    make_probe_block0(source.blocks[0], probe_block0, probe_uid);

    report(ctx, "Testing whether UID can still change...");
    const ProbeOutcome before_lock =
        probe_uid_mutability(nfc, family, source, probe_block0, probe_uid);
    if(before_lock == ProbeOutcome::Blocked) {
        // OTP 2.0, MF4, FUID, and some write-once tags seal themselves when block0 is personalized.
        if(safe_to_lock(nfc, source)) {
            result->ok = true;
            result->locked = true;
            report(ctx, "UID rewrite blocked repeatedly; tag is locked");
        } else {
            report(ctx, "UID probes were blocked, but source verification failed");
        }
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    if(before_lock == ProbeOutcome::Unsafe) {
        report(ctx, "UID probe inconclusive; restoring Bambu block 0...");
        if(!restore_source(nfc, family, source) || !safe_to_lock(nfc, source)) {
            report(ctx, "Probe restore failed; no lock command sent");
        } else {
            report(ctx, "Probe inconclusive; Bambu dump restored, lock aborted");
        }
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "UID changed; restoring Bambu UID/data before lock...");
    if(!restore_source(nfc, family, source) || !safe_to_lock(nfc, source)) {
        report(ctx, "Restore failed; lock aborted to protect block 0");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    bool lock_command_sent = false;
    report(ctx, "Trying family-specific permanent lock...");
    if(family == BrMagicFamily::Ufuid) {
        lock_command_sent = lock_ufuid(nfc, source);
    } else if(family == BrMagicFamily::Gen3) {
        lock_command_sent = lock_gen3(nfc, source);
    } else if(family == BrMagicFamily::Gen4) {
        lock_command_sent = lock_gen4(nfc, source);
    }

    if(lock_command_sent) {
        furi_delay_ms(50);
        if(!safe_to_lock(nfc, source)) {
            report(ctx, "Post-lock Bambu data verification failed");
            nfc_free(nfc);
            if(ctx->dispatcher)
                view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
            return 0;
        }

        report(ctx, "Lock sent; attempting UID change again...");
        const ProbeOutcome after_lock =
            probe_uid_mutability(nfc, family, source, probe_block0, probe_uid);
        if(after_lock == ProbeOutcome::Blocked && safe_to_lock(nfc, source)) {
            result->ok = true;
            result->locked = true;
            report(ctx, "Lock verified: repeated UID rewrites are blocked");
            nfc_free(nfc);
            if(ctx->dispatcher)
                view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
            return 0;
        }

        // Changed or indeterminate: restore the exact selected Bambu image before
        // any fallback. This is mandatory even when the lock command reported OK.
        report(ctx, "Lock not verified; restoring Bambu dump before fallback...");
        if(!restore_source(nfc, family, source) || !safe_to_lock(nfc, source)) {
            report(ctx, "Could not restore Bambu block 0 after lock test");
            nfc_free(nfc);
            if(ctx->dispatcher)
                view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
            return 0;
        }
    }

    // No documented permanent lock for ordinary Gen1A/Gen2/ZUID-style tags. The requested
    // fallback is to leave a valid, non-Bambu blank image rather than expose a mutable Bambu UID
    // that the AMS can attempt to use and potentially invalidate.
    if(blank_after_unlocked(nfc, family, source, ctx)) {
        result->blanked = true;
        result->ok = false;
        report(ctx, "Tag cannot be locked. Left blank. AMS will \"brick\" this mutable tag.");
    } else {
        report(ctx, "Tag cannot be locked; blank fallback also failed");
    }

    nfc_free(nfc);
    if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
    return 0;
}

int32_t br_wipe_worker(void* context) {
    BrWipeContext* ctx = static_cast<BrWipeContext*>(context);
    BrWriteResult* result = ctx ? ctx->result : nullptr;
    if(!ctx || !result) return 0;
    memset(result, 0, sizeof(*result));
    result->family = BrMagicFamily::Unknown;

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        report(ctx, "Could not acquire NFC hardware");
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "Hold the Bambu magic tag steady");
    uint8_t uid[BR_UID_SIZE]{};
    while(!cancelled(ctx) && !wait_uid(nfc, uid))
        furi_delay_ms(120);
    if(cancelled(ctx)) {
        nfc_free(nfc);
        return 0;
    }

    MfClassicType target_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(nfc, &target_type) != MfClassicErrorNone ||
       target_type != MfClassicType1k) {
        report(ctx, "Target is not a MIFARE Classic 1K tag");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "Deriving Bambu A/B keys from the live UID...");
    BrTagDump current{};
    MfClassicDeviceKeys derived{};
    if(!read_bambu_current_dump(nfc, uid, &current, &derived)) {
        report(ctx, "Could not read all 64 blocks with UID-derived Bambu keys");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    const BrMagicFamily family = detect_magic_family_for_wipe(nfc);
    result->family = family;
    snprintf(result->family_name, sizeof(result->family_name), "%s", br_magic_family_name(family));
    if(family == BrMagicFamily::Mf4Unsupported) {
        report(ctx, "MF4 wipe refused: hidden config aliases trailer bytes");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    BrTagDump blank{};
    make_blank_dump(&blank);
    report(ctx, "Resetting block 0 to canonical blank identity first...");
    if(!write_canonical_blank(nfc, family, current, blank)) {
        report(ctx, "Could not reset this tag to the canonical empty magic 1K image");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    report(ctx, "Verifying all 64 blank blocks and default keys...");
    furi_delay_ms(50);
    if(!verify_blank_dump(nfc, blank)) {
        report(ctx, "Wipe write completed, but full blank verification failed");
        nfc_free(nfc);
        if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
        return 0;
    }

    result->ok = true;
    result->blanked = true;
    report(ctx, "Wipe verified: canonical empty magic MIFARE 1K image restored");
    nfc_free(nfc);
    if(ctx->dispatcher) view_dispatcher_send_custom_event(ctx->dispatcher, ctx->done_event);
    return 0;
}
