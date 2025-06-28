#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:RenfeSum10"
typedef struct {
    uint8_t data_sector;
    const MfClassicKeyPair* keys;
} RenfeSum10CardConfig;
static bool renfe_sum10_is_history_entry(const uint8_t* block_data);
static void renfe_sum10_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num);
static void renfe_sum10_parse_travel_history(FuriString* parsed_data, const MfClassicData* data);
static uint32_t renfe_sum10_extract_timestamp(const uint8_t* block_data);
static void renfe_sum10_sort_history_entries(HistoryEntry* entries, int count);
// Keys for RENFE Suma 10 cards - specific keys found in real dumps
const MfClassicKeyPair renfe_sum10_keys[16] = {
    {.a = 0xA8844B0BCA06, .b = 0xffffffffffff}, // Sector 0 - RENFE specific key
    {.a = 0xCB5ED0E57B08, .b = 0xffffffffffff}, // Sector 1 - RENFE specific key 
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 2
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 3
    {.a = 0xC0C1C2C3C4C5, .b = 0xffffffffffff}, // Sector 4 - Alternative RENFE key
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 5 - Value blocks
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 6 - Value blocks
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 7
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 8
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 9
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 10
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 11 - Travel history, Contains blocks 44,45,46
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 12
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 13
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 14
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 15
};

// Alternative common keys for RENFE Suma 10
const MfClassicKeyPair renfe_sum10_alt_keys[16] = {
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 0
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 1
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 2
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 3
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 4
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 5 - Value blocks
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 6 - Value blocks
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 7
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 8
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 9
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 10
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 11
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 12
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 13
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 14
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 15
};


// Check if a block contains a valid history entry
static bool renfe_sum10_is_history_entry(const uint8_t* block_data) {
    // Check for null pointer
    if(!block_data) {
        FURI_LOG_E(TAG, "renfe_sum10_is_history_entry: block_data is NULL");
        return false;
    }
    
    // Very simple check - if byte 1 is 0x98 and byte 0 is not 0x00, consider it a history entry
    // This should catch all the patterns we've seen: 13 98, 1A 98, 1E 98, 16 98, 33 98, 3A 98, 2B 98, 18 98
    
    FURI_LOG_I(TAG, "Checking block pattern: %02X %02X", block_data[0], block_data[1]);
    
    if(block_data[1] == 0x98 && block_data[0] != 0x00) {
        FURI_LOG_I(TAG, "FOUND HISTORY ENTRY: %02X 98", block_data[0]);
        return true;
    }
    
    return false;
}
// Extract timestamp from block data for sorting purposes
static uint32_t renfe_sum10_extract_timestamp(const uint8_t* block_data) {
    if(!block_data) return 0;
    
    // Extract timestamp from bytes 2-4 and combine into a single value for comparison
    uint32_t timestamp = ((uint32_t)block_data[2] << 16) | 
                        ((uint32_t)block_data[3] << 8) | 
                        (uint32_t)block_data[4];
    
    return timestamp;
}
// Sort history entries manually (newest first) - using bubble sort since qsort is not available
static void renfe_sum10_sort_history_entries(HistoryEntry* entries, int count) {
    if(!entries || count <= 1) return;
    
    // Simple bubble sort by timestamp (descending - newest first)
    for(int i = 0; i < count - 1; i++) {
        for(int j = 0; j < count - 1 - i; j++) {
            bool should_swap = false;
            
            // Primary sort: by timestamp (newest first)
            if(entries[j].timestamp < entries[j + 1].timestamp) {
                should_swap = true;
            } else if(entries[j].timestamp == entries[j + 1].timestamp) {
                // Secondary sort: by block number (higher first if timestamps are equal)
                if(entries[j].block_number < entries[j + 1].block_number) {
                    should_swap = true;
                }
            }
            
            if(should_swap) {
                // Swap entries
                HistoryEntry temp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = temp;
            }
        }
    }
}
// Get station name from station code (based on real RENFE Suma 10 data analysis)
static const char* renfe_sum10_get_station_name(uint16_t station_code) {
    switch(station_code) {
        // CODIGOS REALES ENCONTRADOS EN LA TARJETA
        case 0x7069: return "Valencia-Nord";
        case 0x0080: return "Xativa";           
        case 0x9400: return "Jesus";
        case 0x6900: return "Turia";
        case 0x6F00: return "Pont de Fusta";
        case 0x0100: return "Alameda";
        
        // CODIGOS NUEVOS ENCONTRADOS EN LOGS DE TARJETA REAL
        case 0xA002: return "Estacio del Nord";  // Codigo real 0xA002
        case 0x7882: return "Xativa-Estacio";    // Codigo real 0x7882
        case 0x7002: return "Cullera";           // Codigo real 0x7002
        case 0x3891: return "L'Eliana";          // Codigo real 0x3891
        case 0x086E: return "Valencia Sud";      // Codigo real 0x086E
        case 0x3C91: return "Benimamet";         // Codigo real 0x3C91
        case 0x2894: return "Sagunto";           // Codigo real 0x2894 - Nuevo encontrado
        case 0xA46F: return "Alzira";            // Codigo real 0xA46F - Nuevo encontrado
        case 0x3811: return "Valencia-Joaquin Sorolla"; // Codigo real 0x3811 - Nuevo encontrado
        
        // CODIGOS ADICIONALES ENCONTRADOS EN ANALISIS
        case 0x1800: return "Angel Guimera";    // Codigo real encontrado
        case 0x1900: return "Bailen";           // Posible codigo real
        case 0x1A00: return "Colon";            // Posible codigo real
        case 0x6980: return "Alacant";          // Variante del codigo
        case 0x6A00: return "Russafa";          // Posible codigo real
        
        // CODIGOS ESTIMADOS PARA ESTACIONES PRINCIPALES DE VALENCIA
        // Metro Linea 1
        case 0x1000: return "Betera";
        case 0x1100: return "Massarrojos";
        case 0x1200: return "Godella";
        case 0x1300: return "Burjassot-Godella";
        case 0x1400: return "Burjassot";
        case 0x1500: return "Empalme";
        case 0x1600: return "Campanar";
        case 0x1C00: return "Facultats-Manuel Broseta";
        case 0x1D00: return "Benimaclet";
        case 0x1E00: return "Machado";
        case 0x1F00: return "Alboraia Palmaret";
        case 0x2000: return "Alboraia Peris Arago";
        case 0x2100: return "Almassera";
        case 0x2200: return "Meliana";
        case 0x2300: return "Foios";
        case 0x2400: return "Albalat dels Sorells";
        case 0x2500: return "Museros";
        case 0x2600: return "Massamagrell";
        case 0x2700: return "La Pobla de Farnals";
        case 0x2800: return "Rafelbunyol";
        case 0x2900: return "Castello";
        
        // Metro Linea 2
        case 0x3000: return "Lliria";
        case 0x3100: return "Fondo de Benaguasil";
        case 0x3200: return "Benaguasil";
        case 0x3300: return "La Pobla de Vallbona";
        case 0x3400: return "Gallipont-Torre del Virrei";
        case 0x3500: return "L'Eliana";
        case 0x4000: return "Benimamet";
        case 0x4100: return "Cantereria";
        case 0x4200: return "Beniferri";
        case 0x4300: return "Pl. Espanya";
        case 0x4500: return "Patraix";
        case 0x4600: return "Safranar";
        case 0x4700: return "Sant Isidre";
        case 0x4800: return "Valencia Sud";
        case 0x4900: return "Paiporta";
        case 0x4A00: return "Picanya";
        case 0x4B00: return "Torrent";
        case 0x4C00: return "Torrent Avinguda";
        
        // Metro Linea 3/5/9
        case 0x5100: return "Av. del Cid";
        case 0x5200: return "Nou d'Octubre";
        case 0x5300: return "Mislata";
        case 0x5400: return "Mislata Almassil";
        case 0x5500: return "Faitanar";
        case 0x5600: return "Quart de Poblet";
        case 0x5700: return "Salt de l'Aigua";
        case 0x5800: return "Manises";
        case 0x5900: return "Roses";
        case 0x5A00: return "Aeroport";
        
        // Linea 5/7
        case 0x6000: return "Maritim";
        case 0x6100: return "Amistat";
        case 0x6200: return "Ayora";
        case 0x6300: return "Arago";
        case 0x7000: return "Bailen";
        
        // Linea 9
        case 0x8100: return "La Cova";
        case 0x8200: return "La Presa";
        case 0x8300: return "Valencia la Vella";
        case 0x8400: return "Masia de Traver";
        case 0x8500: return "Riba-roja de Turia";
        
        // Tranvia Linea 4
        case 0x9000: return "Mas del Rosari";
        case 0x9100: return "Tomas y Valiente";
        case 0x9200: return "Parc Cientific";
        case 0x9300: return "Lloma Llarga-Terramelar";
        case 0x9500: return "Fira Valencia";
        case 0x9B00: return "Sagunt";
        case 0x9D00: return "Trinitat";
        case 0x9E00: return "Vicente Zaragoza";
        case 0x9F00: return "Universitat Politecnica";
        case 0xA000: return "La Carrasca";
        case 0xA100: return "Tarongers-Ernest Lluch";
        case 0xA200: return "Betero";
        case 0xA300: return "La Cadena";
        case 0xA400: return "Platja Malva-rosa";
        case 0xA500: return "Platja les Arenes";
        case 0xA600: return "Cabanyal";
        case 0xA700: return "Dr. Lluch";
        
        // Tranvia Linea 6/8
        case 0xB000: return "Tossal del Rei";
        case 0xB100: return "Neptu";
        case 0xB200: return "Grau-La Marina";
        case 0xB300: return "Francesc Cubells";
        
        // Tranvia Linea 10  
        case 0xC000: return "Alacant";
        case 0xC100: return "Russafa";
        case 0xC200: return "Amado Granell-Montolivet";
        case 0xC300: return "Quatre Carreres";
        case 0xC400: return "Ciutat Arts i Ciencies-Justicia";
        case 0xC500: return "Oceanografic";
        case 0xC600: return "Moreres";
        case 0xC700: return "Natzaret";
        
        // RENFE Cercanias Valencia
        case 0xD000: return "Valencia-Estacio del Nord";
        case 0xD100: return "Valencia-Cabanyal";
        case 0xD200: return "Valencia-Fuente San Luis";
        case 0xD300: return "Sagunto (RENFE)";
        case 0xD400: return "Castellon (RENFE)";
        case 0xD500: return "Xativa (RENFE)";
        case 0xD600: return "Alzira";
        case 0xD700: return "Cullera";
        case 0xD800: return "Gandia";
        case 0xD900: return "Denia";
        
        // If station code is 0, don't show station info
        case 0x0000: return "";
        
        default: return "Unknown";
    }
}

// Parse a single history entry
// Parse a single history entry
static void renfe_sum10_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num) {
    // Check for null pointers
    if(!parsed_data || !block_data) {
        FURI_LOG_E(TAG, "renfe_sum10_parse_history_entry: NULL pointer - parsed_data=%p, block_data=%p", 
                   (void*)parsed_data, (void*)block_data);
        return;
    }
    
    // Extract transaction type from first byte
    uint8_t transaction_type = block_data[0];
    
    /* TRANSACTION TYPES MAPPING (based on analysis):
     * CONFIRMED TYPES:
     * 0x13 = ENTRY     - Entrada al sistema
     * 0x1A = EXIT      - Salida del sistema  
     * 0x1E = TRANSFER  - Transbordo entre lineas
     * 0x16 = VALIDATION- Validacion de titulo
     * 0x33 = TOP-UP    - Recarga de saldo
     * 0x3A = CHARGE    - Carga/cargo adicional
     * 0x18 = CHECK     - Verificacion
     * 0x2B = SPECIAL   - Operacion especial
     * 
     * DEDUCED TYPES (found in real cards):
     * 0x17 = INSPECTION- Posible control/revision de inspector
     * 0x23 = DISCOUNT  - Posible aplicacion de descuento
     * 0x2A = PENALTY   - Posible sancion o multa
     */
    
    // Extract timestamp data (bytes 2-4)
    uint8_t timestamp_1 = block_data[2];
    uint8_t timestamp_2 = block_data[3];
    uint8_t timestamp_3 = block_data[4];
    
    // Extract station codes (bytes 9-10) - Read as big-endian
    uint16_t station_code = (block_data[9] << 8) | block_data[10];
    
    // Log the station code for debugging
    FURI_LOG_I(TAG, "Station code bytes: [%d][%d] = 0x%02X 0x%02X -> 0x%04X", 
               9, 10, block_data[9], block_data[10], station_code);
    
    // Extract transaction details
    uint8_t detail1 = block_data[5];
    uint8_t detail2 = block_data[6];
    
    // Format the entry
    furi_string_cat_printf(parsed_data, "%d. ", entry_num);
    
    // Interpret transaction type
    switch(transaction_type) {
        case 0x13:
            furi_string_cat_printf(parsed_data, "ENTRY");
            break;
        case 0x1A:
            furi_string_cat_printf(parsed_data, "EXIT");
            break;
        case 0x1E:
            furi_string_cat_printf(parsed_data, "TRANSFER");
            break;
        case 0x16:
            furi_string_cat_printf(parsed_data, "VALIDATION");
            break;
        case 0x17:
            furi_string_cat_printf(parsed_data, "INSPECTION");  // Posible control/revision
            break;
        case 0x23:
            furi_string_cat_printf(parsed_data, "DISCOUNT");    // Posible descuento aplicado
            break;
        case 0x2A:
            furi_string_cat_printf(parsed_data, "PENALTY");     // Posible sancion/multa
            break;
        case 0x33:
            furi_string_cat_printf(parsed_data, "TOP-UP");
            break;
        case 0x3A:
            furi_string_cat_printf(parsed_data, "CHARGE");
            break;
        case 0x18:
            furi_string_cat_printf(parsed_data, "CHECK");
            break;
        case 0x2B:
            furi_string_cat_printf(parsed_data, "SPECIAL");
            break;
        default:
            furi_string_cat_printf(parsed_data, "TYPE_%02X", transaction_type);
            break;
    }
    // Add station information
    const char* station_name = renfe_sum10_get_station_name(station_code);
    if(station_code != 0x0000 && strlen(station_name) > 0) {
        furi_string_cat_printf(parsed_data, " at %s", station_name);
        if(strcmp(station_name, "Unknown") == 0) {
            furi_string_cat_printf(parsed_data, " (0x%04X)", station_code);
            // Log unknown station codes for mapping
            FURI_LOG_W(TAG, "UNKNOWN STATION CODE: 0x%04X (bytes: 0x%02X 0x%02X)", 
                       station_code, block_data[9], block_data[10]);
        } else {
            FURI_LOG_I(TAG, "Station mapped: %s (0x%04X)", station_name, station_code);
        }
    }
    
static bool renfe_sum10_get_card_config(RenfeSum10CardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 5; // Primary data sector for trips
        config->keys = renfe_sum10_keys;
    } else if(type == MfClassicType4k) {
        // Mifare Plus 2K/4K configured as 1K for RENFE Suma 10 compatibility
        // Treat it exactly like a 1K card - only use the first 1K sectors
        config->data_sector = 5; // Same as 1K - RENFE only uses first 1K sectors
        config->keys = renfe_sum10_keys;
        FURI_LOG_I(TAG, "RENFE Suma 10: Treating Mifare Plus as 1K card for compatibility");
    } else {
        success = false; 
    }

    return success;
}

static bool renfe_sum10_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        // Verify card type - accepts both 1K and Plus (configured as 1K)
        RenfeSum10CardConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        if(!renfe_sum10_get_card_config(&cfg, data->type)) {
            FURI_LOG_E(TAG, "RENFE Suma 10: Unsupported card type %d", data->type);
            break;
        }

        furi_string_cat_printf(parsed_data, "\e#RENFE Suma 10\n");
        
        // Show card type info
        if(data->type == MfClassicType1k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Classic 1K\n");
        } else if(data->type == MfClassicType4k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Plus 2K (as 1K)\n");
        } else {
            furi_string_cat_printf(parsed_data, "Type: Unknown (%d)\n", data->type);
        }
        
        // Extract UID
        const uint8_t* uid = data->iso14443_3a_data->uid;
        size_t uid_len = data->iso14443_3a_data->uid_len;
        
        furi_string_cat_printf(parsed_data, "UID: ");
        for(size_t i = 0; i < uid_len; i++) {
            furi_string_cat_printf(parsed_data, "%02X", uid[i]);
            if(i < uid_len - 1) furi_string_cat_printf(parsed_data, ":");
        }
        furi_string_cat_printf(parsed_data, "\n");
        
        // Extract trips from Block 5 (based on real dump analysis)
        const uint8_t* block5 = data->block[5].data;
        if(block5[0] == 0x01 && block5[1] == 0x00 && block5[2] == 0x00 && block5[3] == 0x00) {
            // Extract trip count from byte 4
            int viajes = (int)block5[4];
            furi_string_cat_printf(parsed_data, "Trips: %d\n", viajes);
        } else {
            furi_string_cat_printf(parsed_data, "Trips: Not available\n");
        }
        
        parsed = true;
        
    } while(false);

    return parsed;
}

static bool renfe_sum10_checked = false;

static NfcCommand renfe_sum10_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        command = NfcCommandContinue;
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        app->sec_num = 0;
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;

    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicKey key = {0};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        bit_lib_num_to_bytes_be(renfe_sum10_keys[app->sec_num].a, COUNT_OF(key.data), key.data);
          if(!renfe_sum10_checked) {
            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            app->sec_num++;
            renfe_sum10_checked = true;
        }
        
        nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        
        if(mfc_data->type == MfClassicType1k) {
            bit_lib_num_to_bytes_be(renfe_sum10_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            if(app->sec_num == 16) {
                mfc_event->data->read_sector_request_data.key_provided = false;
                app->sec_num = 0;
            }
            app->sec_num++;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        FuriString* parsed_data = furi_string_alloc();
        Widget* widget = app->widget;
        
        if(!renfe_sum10_parse(parsed_data, mfc_data)) {
            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        metroflip_app_blink_stop(app);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void renfe_sum10_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!renfe_sum10_parse(parsed_data, mfc_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            mf_classic_free(mfc_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, renfe_sum10_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool renfe_sum10_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void renfe_sum10_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin renfe_sum10_plugin = {
    .card_name = "RENFE Suma 10",
    .plugin_on_enter = renfe_sum10_on_enter,
    .plugin_on_event = renfe_sum10_on_event,
    .plugin_on_exit = renfe_sum10_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor renfe_sum10_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &renfe_sum10_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* renfe_sum10_plugin_ep(void) {
    return &renfe_sum10_plugin_descriptor;
}
