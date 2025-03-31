#include "passy_reader.h"

#define TAG "PassyReader"

static uint8_t passport_aid[] = {0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};
static uint8_t select_header[] = {0x00, 0xA4, 0x04, 0x0C};

static uint8_t get_challenge[] = {0x00, 0x84, 0x00, 0x00, 0x08};

static uint8_t SW_success[] = {0x90, 0x00};

size_t asn1_length(uint8_t data[3]) {
    if(data[0] <= 0x7F) {
        return data[0];
    } else if(data[0] == 0x81) {
        return data[1];
    } else if(data[0] == 0x82) {
        return (data[1] << 8) | data[2];
    }
    return 0;
}

PassyReader* passy_reader_alloc(Passy* passy, Iso14443_4bPoller* iso14443_4b_poller) {
    PassyReader* passy_reader = malloc(sizeof(PassyReader));
    memset(passy_reader, 0, sizeof(PassyReader));

    passy_reader->iso14443_4b_poller = iso14443_4b_poller;

    passy_reader->DG1 = passy->DG1;
    passy_reader->tx_buffer = bit_buffer_alloc(PASSY_READER_MAX_BUFFER_SIZE);
    passy_reader->rx_buffer = bit_buffer_alloc(PASSY_READER_MAX_BUFFER_SIZE);

    char passport_number[11];
    memset(passport_number, 0, sizeof(passport_number));
    memcpy(passport_number, passy->passport_number, strlen(passy->passport_number));
    passport_number[strlen(passy->passport_number)] = passy_checksum(passy->passport_number);
    FURI_LOG_I(TAG, "Passport number: %s", passport_number);

    char date_of_birth[8];
    memset(date_of_birth, 0, sizeof(date_of_birth));
    memcpy(date_of_birth, passy->date_of_birth, strlen(passy->date_of_birth));
    date_of_birth[strlen(passy->date_of_birth)] = passy_checksum(passy->date_of_birth);
    FURI_LOG_I(TAG, "Date of birth: %s", date_of_birth);

    char date_of_expiry[8];
    memset(date_of_expiry, 0, sizeof(date_of_expiry));
    memcpy(date_of_expiry, passy->date_of_expiry, strlen(passy->date_of_expiry));
    date_of_expiry[strlen(passy->date_of_expiry)] = passy_checksum(passy->date_of_expiry);
    FURI_LOG_I(TAG, "Date of expiry: %s", date_of_expiry);

    passy_save_mrz_info(passy);

    passy_reader->secure_messaging = secure_messaging_alloc(
        (uint8_t*)passport_number, (uint8_t*)date_of_birth, (uint8_t*)date_of_expiry);

    return passy_reader;
}

void passy_reader_free(PassyReader* passy_reader) {
    furi_assert(passy_reader);
    bit_buffer_free(passy_reader->tx_buffer);
    bit_buffer_free(passy_reader->rx_buffer);
    if(passy_reader->secure_messaging) {
        secure_messaging_free(passy_reader->secure_messaging);
    }
    free(passy_reader);
}

NfcCommand passy_reader_select_application(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;

    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;
    Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
    Iso14443_4bError error;

    bit_buffer_append_bytes(tx_buffer, select_header, sizeof(select_header));
    bit_buffer_append_byte(tx_buffer, sizeof(passport_aid));
    bit_buffer_append_bytes(tx_buffer, passport_aid, sizeof(passport_aid));
    bit_buffer_append_byte(tx_buffer, 0x00); // Le

    error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    passy_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    return ret;
}

NfcCommand passy_reader_get_challenge(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;

    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;
    Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
    Iso14443_4bError error;

    bit_buffer_append_bytes(tx_buffer, get_challenge, sizeof(get_challenge));

    error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    passy_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    SecureMessaging* secure_messaging = passy_reader->secure_messaging;
    const uint8_t* rnd_icc = data;
    memcpy(secure_messaging->rndICC, rnd_icc, 8);

    return ret;
}

NfcCommand passy_reader_authenticate(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;
    Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
    Iso14443_4bError error;

    // TODO: move into secure_messaging
    SecureMessaging* secure_messaging = passy_reader->secure_messaging;
    uint8_t S[32];
    memset(S, 0, sizeof(S));
    uint8_t eifd[32];
    memcpy(S, secure_messaging->rndIFD, sizeof(secure_messaging->rndIFD));
    memcpy(
        S + sizeof(secure_messaging->rndIFD),
        secure_messaging->rndICC,
        sizeof(secure_messaging->rndICC));
    memcpy(
        S + sizeof(secure_messaging->rndIFD) + sizeof(secure_messaging->rndICC),
        secure_messaging->Kifd,
        sizeof(secure_messaging->Kifd));

    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));
    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    mbedtls_des3_set2key_enc(&ctx, secure_messaging->KENC);
    mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, sizeof(S), iv, S, eifd);
    mbedtls_des3_free(&ctx);

    passy_log_buffer(TAG, "S", S, sizeof(S));
    passy_log_buffer(TAG, "eifd", eifd, sizeof(eifd));

    uint8_t mifd[8];
    passy_mac(secure_messaging->KMAC, eifd, sizeof(eifd), mifd, false);
    passy_log_buffer(TAG, "mifd", mifd, sizeof(mifd));

    uint8_t authenticate_header[] = {0x00, 0x82, 0x00, 0x00};

    bit_buffer_append_bytes(tx_buffer, authenticate_header, sizeof(authenticate_header));
    bit_buffer_append_byte(tx_buffer, sizeof(eifd) + sizeof(mifd));
    bit_buffer_append_bytes(tx_buffer, eifd, sizeof(eifd));
    bit_buffer_append_bytes(tx_buffer, mifd, sizeof(mifd));
    bit_buffer_append_byte(tx_buffer, 0x28); // Le

    passy_log_bitbuffer(TAG, "NFC transmit", tx_buffer);

    error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    passy_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    const uint8_t* mac = data + length - 2 - 8;
    uint8_t calculated_mac[8];
    passy_mac(secure_messaging->KMAC, (uint8_t*)data, length - 8 - 2, calculated_mac, false);
    if(memcmp(mac, calculated_mac, sizeof(calculated_mac)) != 0) {
        FURI_LOG_W(TAG, "Invalid MAC");
        return NfcCommandStop;
    }

    uint8_t decrypted[32];
    do {
        uint8_t iv[8];
        memset(iv, 0, sizeof(iv));

        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_dec(&ctx, secure_messaging->KENC);
        mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, length - 2 - 8, iv, data, decrypted);
        mbedtls_des3_free(&ctx);
    } while(false);
    passy_log_buffer(TAG, "decrypted", decrypted, sizeof(decrypted));

    uint8_t* rnd_icc = decrypted;
    uint8_t* rnd_ifd = decrypted + 8;
    uint8_t* Kicc = decrypted + 16;

    if(memcmp(rnd_icc, secure_messaging->rndICC, sizeof(secure_messaging->rndICC)) != 0) {
        FURI_LOG_W(TAG, "Invalid rndICC");
        return NfcCommandStop;
    }

    memcpy(secure_messaging->Kicc, Kicc, sizeof(secure_messaging->Kicc));
    memcpy(secure_messaging->SSC + 0, rnd_icc + 4, 4);
    memcpy(secure_messaging->SSC + 4, rnd_ifd + 4, 4);

    return ret;
}

NfcCommand passy_reader_select_file(PassyReader* passy_reader, uint16_t file_id) {
    NfcCommand ret = NfcCommandContinue;

    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;
    Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
    Iso14443_4bError error;

    uint8_t select_0101[] = {0x00, 0xa4, 0x02, 0x0c, 0x02, 0x00, 0x00};
    select_0101[5] = (file_id >> 8) & 0xFF;
    select_0101[6] = file_id & 0xFF;

    secure_messaging_wrap_apdu(
        passy_reader->secure_messaging, select_0101, sizeof(select_0101), tx_buffer);

    passy_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
    error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    passy_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    secure_messaging_unwrap_rapdu(passy_reader->secure_messaging, rx_buffer);
    passy_log_bitbuffer(TAG, "NFC response (decrypted)", rx_buffer);

    return ret;
}

NfcCommand passy_reader_read_binary(
    PassyReader* passy_reader,
    uint8_t offset,
    uint8_t Le,
    uint8_t* output_buffer) {
    NfcCommand ret = NfcCommandContinue;

    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;
    Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
    Iso14443_4bError error;

    uint8_t read_binary[] = {0x00, 0xB0, 0x00, offset, Le};

    secure_messaging_wrap_apdu(
        passy_reader->secure_messaging, read_binary, sizeof(read_binary), tx_buffer);

    passy_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
    error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    passy_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    secure_messaging_unwrap_rapdu(passy_reader->secure_messaging, rx_buffer);
    passy_log_bitbuffer(TAG, "NFC response (decrypted)", rx_buffer);

    const uint8_t* decrypted_data = bit_buffer_get_data(rx_buffer);
    memcpy(output_buffer, decrypted_data, Le);

    return ret;
}

NfcCommand passy_reader_state_machine(Passy* passy, PassyReader* passy_reader) {
    furi_assert(passy_reader);
    NfcCommand ret = NfcCommandContinue;

    do {
        ret = passy_reader_select_application(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        ret = passy_reader_get_challenge(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        ret = passy_reader_authenticate(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        FURI_LOG_I(TAG, "Mututal authentication success");
        secure_messaging_calculate_session_keys(passy_reader->secure_messaging);
        view_dispatcher_send_custom_event(
            passy->view_dispatcher, PassyCustomEventReaderAuthenticated);

        ret = passy_reader_select_file(passy_reader, 0x0101);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }

        bit_buffer_reset(passy->DG1);
        uint8_t header[4];
        ret = passy_reader_read_binary(passy_reader, 0x00, 0x04, header);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);

            break;
        }
        size_t body_size = asn1_length(header + 1);
        uint8_t body_offset = 0x04;
        do {
            view_dispatcher_send_custom_event(
                passy->view_dispatcher, PassyCustomEventReaderReading);
            uint8_t chunk[0x20];
            uint8_t Le = MIN(sizeof(chunk), (size_t)(body_size - body_offset));

            ret = passy_reader_read_binary(passy_reader, body_offset, Le, chunk);
            if(ret != NfcCommandContinue) {
                view_dispatcher_send_custom_event(
                    passy->view_dispatcher, PassyCustomEventReaderError);
                break;
            }
            bit_buffer_append_bytes(passy_reader->DG1, chunk, sizeof(chunk));
            body_offset += sizeof(chunk);
        } while(body_offset < body_size);
        passy_log_bitbuffer(TAG, "DG1", passy_reader->DG1);

        ret = passy_reader_select_file(passy_reader, 0x0102);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }

        ret = passy_reader_read_binary(passy_reader, 0x00, 0x04, header);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        body_size = asn1_length(header + 1);
        FURI_LOG_I(TAG, "DG2 length: %d", body_size);

        // Everything done
        ret = NfcCommandStop;
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderSuccess);
    } while(false);

    return ret;
}

NfcCommand passy_reader_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4b);
    Passy* passy = context;
    NfcCommand ret = NfcCommandContinue;

    const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;
    Iso14443_4bPoller* iso14443_4b_poller = event.instance;

    FURI_LOG_D(TAG, "iso14443_4b_event->type %i", iso14443_4b_event->type);

    PassyReader* passy_reader = passy_reader_alloc(passy, iso14443_4b_poller);

    if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderDetected);
        nfc_device_set_data(
            passy->nfc_device, NfcProtocolIso14443_4b, nfc_poller_get_data(passy->poller));

        ret = passy_reader_state_machine(passy, passy_reader);

        furi_thread_set_current_priority(FuriThreadPriorityLowest);
    } else if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeError) {
        Iso14443_4bPollerEventData* data = iso14443_4b_event->data;
        Iso14443_4bError error = data->error;
        FURI_LOG_W(TAG, "Iso14443_4bError %i", error);
        switch(error) {
        case Iso14443_4bErrorNone:
            break;
        case Iso14443_4bErrorNotPresent:
            break;
        case Iso14443_4bErrorProtocol:
            ret = NfcCommandStop;
            break;
        case Iso14443_4bErrorTimeout:
            break;
        }
    }

    passy_reader_free(passy_reader);
    return ret;
}
