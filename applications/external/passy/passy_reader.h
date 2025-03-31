#pragma once

#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4b/iso14443_4b_poller.h>
#include <lib/nfc/helpers/iso14443_crc.h>
#include <mbedtls/des.h>

#include "passy_i.h"
#include "passy_common.h"
#include "secure_messaging.h"

#define PASSY_READER_MAX_BUFFER_SIZE 128

NfcCommand passy_reader_poller_callback(NfcGenericEvent event, void* context);

typedef struct {
    Iso14443_4bPoller* iso14443_4b_poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    BitBuffer* DG1;

    SecureMessaging* secure_messaging;

} PassyReader;

PassyReader* passy_reader_alloc(Passy* passy, Iso14443_4bPoller* iso14443_4b_poller);

void passy_reader_free(PassyReader* passy_reader);

void passy_reader_mac(uint8_t* key, uint8_t* data, size_t data_length, uint8_t* mac);
