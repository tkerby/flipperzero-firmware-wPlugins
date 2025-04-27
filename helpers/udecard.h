/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025 Alexander Hahn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef UDECARD_H
#define UDECARD_H

#include <furi.h>

#include <nfc/nfc_device.h>

#define BLOCKS_PER_SECTOR 4

#define UDECARD_UID_BLOCK (0 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_UID_SIZE  4

// used to identify whether the card is a UDECard
#define UDECARD_CONSTANT_BLOCK1 (0 * BLOCKS_PER_SECTOR + 1)
#define UDECARD_CONSTANT_BLOCK1_CONTENTS_BYTES \
    {0x55, 0x06, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a}
static const uint8_t UDECARD_CONSTANT_BLOCK1_CONTENTS[] = UDECARD_CONSTANT_BLOCK1_CONTENTS_BYTES;
#define UDECARD_CONSTANT_BLOCK1_SIZE 16

#define UDECARD_CONSTANT_BLOCK2 (0 * BLOCKS_PER_SECTOR + 2)
#define UDECARD_CONSTANT_BLOCK2_CONTENTS_BYTES \
    {0x38, 0x2a, 0x38, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
static const uint8_t UDECARD_CONSTANT_BLOCK2_CONTENTS[] = UDECARD_CONSTANT_BLOCK2_CONTENTS_BYTES;
#define UDECARD_CONSTANT_BLOCK2_SIZE 16

// membertype
#define UDECARD_MEMBERTYPE_BLOCK              (1 * BLOCKS_PER_SECTOR + 2)
#define UDECARD_MEMBERTYPE_BYTE_IN_BLOCK_1OCC 0
#define UDECARD_MEMBERTYPE_BYTE_IN_BLOCK_2OCC 2
#define UDECARD_MEMBER_TYPE_STUDENT_BYTE      0x02
#define UDECARD_MEMBER_TYPE_EMPLOYEE_BYTE     0x03

// balance
#define UDECARD_BALANCE_BLOCK_1OCC (3 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_BALANCE_BYTE_1OCC  2
#define UDECARD_BALANCE_BLOCK_2OCC (3 * BLOCKS_PER_SECTOR + 1)
#define UDECARD_BALANCE_BYTE_2OCC  7

// transaction counter
#define UDECARD_TRANSCOUNT_BLOCK_1OCC (2 * BLOCKS_PER_SECTOR + 2)
#define UDECARD_TRANSCOUNT_BYTE_1OCC  13
#define UDECARD_TRANSCOUNT_BLOCK_2OCC (3 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_TRANSCOUNT_BYTE_2OCC  10

// student number
#define UDECARD_MEMBER_NUMBER_BLOCK_1OCC (5 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_MEMBER_NUMBER_BLOCK_2OCC (6 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_MEMBER_NUMBER_BLOCK_3OCC (8 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_MEMBER_NUMBER_BLOCK_4OCC (9 * BLOCKS_PER_SECTOR + 0)
#define UDECARD_MEMBER_NUMBER_SIZE       12

// keys
// these are widely present in a lot of dictionaries (including the Flipper’s),
// therefore they cannot be really considered private.
// They only allow reading, no modification.
// I am in no way publishing them, I am just using them.
#define UDECARD_KEY_SIZE 6

#define UDECARD_KEYA_0_BYTES {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}
#define UDECARD_KEYA_1_BYTES {0xA2, 0x7D, 0x38, 0x04, 0xC2, 0x59}
#define UDECARD_KEYA_2_BYTES {0x52, 0x8C, 0x9D, 0xFF, 0xE2, 0x8C}
#define UDECARD_KEYA_3_BYTES {0x3E, 0x35, 0x54, 0xAF, 0x0E, 0x12}
#define UDECARD_KEYA_4_BYTES {0x74, 0x0E, 0x9A, 0x4F, 0x9A, 0xAF}
#define UDECARD_KEYA_5_BYTES {0x97, 0x18, 0x4D, 0x13, 0x62, 0x33}

#define UDECARD_KSNR_SIZE_MAX_LENGTH 10

#include <nfc/protocols/mf_classic/mf_classic.h>

// AFAIK the University also issues cash cards (for payment only)
// and library cards (for borrowing books).
// Pull requests or additional information is very appreciated!
typedef enum UDECardMemberType {
    UDECardMemberTypeStudent,
    UDECardMemberTypeEmployee,
    UDECardMemberTypeUnknown,
} UDECardMemberType;

#define UDECARD_MEMBER_TYPE_TO_STRING(X)             \
    ((X) == UDECardMemberTypeStudent  ? "Student" :  \
     (X) == UDECardMemberTypeEmployee ? "Employee" : \
                                        "Unknown")

typedef enum UDECardLoadingResult {
    UDECardLoadingResultSuccess,
    UDECardLoadingResultErrorNotNFC,
    UDECardLoadingResultErrorNotMfClassic,
} UDECardLoadingResult;

typedef enum UDECardParsingResult {
    UDECardParsingResultSuccess = 0,
    UDECardParsingResultErrorMagic = 1 << 0,
    UDECardParsingResultErrorKsnr = 1 << 1,
    UDECardParsingResultErrorMemberType = 1 << 2,
    UDECardParsingResultErrorBalance = 1 << 3,
    UDECardParsingResultErrorMemberNumber = 1 << 4,
    UDECardParsingResultErrorTransactionCount = 1 << 5
} UDECardParsingResult;

typedef struct UDECard {
    MfClassicData* carddata;

    UDECardLoadingResult loading_result;
    UDECardParsingResult parsing_result;

    uint8_t uid[4];

    char ksnr[UDECARD_KSNR_SIZE_MAX_LENGTH + 1]; // KS-Nr. (Karten-Seriennummer)
    UDECardMemberType member_type; // Stundent or Employee
    char member_number
        [UDECARD_MEMBER_NUMBER_SIZE +
         1]; // student number for students, employee number for employees
    uint16_t balance;
    uint16_t transaction_count;
} UDECard;

UDECard* udecard_alloc();
void udecard_free(UDECard* udecard);
UDECardParsingResult udecard_parse(UDECard* udecard, MfClassicData* mfclassicdata);
void uid_to_ksnr(char* ksnr, uint8_t* uid);
int xor3_to_int(const uint8_t* balance_data);

UDECardLoadingResult udecard_load_from_nfc_device(UDECard* udecard, NfcDevice* nfc_device);
UDECardLoadingResult udecard_load_from_path(UDECard* udecard, FuriString* path);

char* udecard_loading_error_string(UDECardLoadingResult loading_result);
char* udecard_parsing_error_string(UDECardParsingResult parsing_result);

#endif
