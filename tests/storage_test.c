/* In-memory SD fixture: execute the production scanner, reader and NDEF encoder. */
#include <assert.h>
#include <furi.h>
#include <storage/storage.h>

void storage_get_next_filename(Storage*, const char*, const char*, const char*, FuriString*, size_t);
#include "../src/nfc_qr_storage.c"

typedef struct {
    char path[128];
    uint8_t data[512];
    size_t length;
    bool used;
} TestFile;
static TestFile files[128];
static Storage sd;

size_t test_strlcpy(char* dest, const char* source, size_t size) {
    size_t length = strlen(source);
    if(size) {
        size_t copied = length < size - 1 ? length : size - 1;
        memcpy(dest, source, copied);
        dest[copied] = 0;
    }
    return length;
}
FuriString* furi_string_alloc(void) {
    return calloc(1, sizeof(FuriString));
}
void furi_string_free(FuriString* value) {
    free(value);
}
const char* furi_string_get_cstr(FuriString* value) {
    return value->text;
}
static int find(const char* path) {
    for(size_t i = 0; i < 128; i++)
        if(files[i].used && !strcasecmp(files[i].path, path)) return (int)i;
    return -1;
}
static int put(const char* path, const void* data, size_t length) {
    assert(length <= sizeof(files[0].data));
    int slot = find(path);
    if(slot < 0) {
        for(size_t i = 0; i < 128; i++)
            if(!files[i].used) {
                slot = (int)i;
                break;
            }
    }
    assert(slot >= 0);
    files[slot].used = true;
    strlcpy(files[slot].path, path, sizeof(files[slot].path));
    memcpy(files[slot].data, data, length);
    files[slot].length = length;
    return slot;
}
File* storage_file_alloc(Storage* storage) {
    (void)storage;
    File* file = calloc(1, sizeof(File));
    file->index = -1;
    return file;
}
void storage_file_free(File* file) {
    free(file);
}
bool storage_file_open(File* file, const char* path, int access, int mode) {
    (void)access;
    file->position = 0;
    file->index = mode == FSOM_CREATE_ALWAYS ? put(path, "", 0) : find(path);
    return file->index >= 0;
}
bool storage_file_close(File* file) {
    (void)file;
    return true;
}
size_t storage_file_read(File* file, void* data, size_t length) {
    size_t remaining = files[file->index].length - file->position;
    if(length > remaining) length = remaining;
    memcpy(data, files[file->index].data + file->position, length);
    file->position += length;
    return length;
}
size_t storage_file_write(File* file, const void* data, size_t length) {
    assert(length <= sizeof(files[0].data));
    memcpy(files[file->index].data, data, length);
    files[file->index].length = length;
    return length;
}
uint64_t storage_file_size(File* file) {
    return files[file->index].length;
}
bool storage_file_sync(File* file) {
    (void)file;
    return true;
}
bool storage_dir_open(File* file, const char* path) {
    (void)path;
    file->directory_index = 0;
    return true;
}
bool storage_dir_read(File* file, FileInfo* info, char* name, size_t size) {
    while(file->directory_index < 128) {
        TestFile* item = &files[file->directory_index++];
        if(!item->used || strncmp(item->path, NFC_QR_DATA_DIR "/", strlen(NFC_QR_DATA_DIR) + 1))
            continue;
        info->directory = false;
        strlcpy(name, strrchr(item->path, '/') + 1, size);
        return true;
    }
    return false;
}
bool storage_dir_close(File* file) {
    (void)file;
    return true;
}
bool file_info_is_dir(const FileInfo* info) {
    return info->directory;
}
bool storage_common_exists(Storage* storage, const char* path) {
    (void)storage;
    return find(path) >= 0;
}
bool storage_simply_mkdir(Storage* storage, const char* path) {
    (void)storage;
    (void)path;
    return true;
}
bool storage_simply_remove(Storage* storage, const char* path) {
    (void)storage;
    int index = find(path);
    if(index >= 0) files[index].used = false;
    return true;
}
void storage_get_next_filename(
    Storage* storage,
    const char* dir,
    const char* name,
    const char* ext,
    FuriString* out,
    size_t size) {
    (void)storage;
    (void)dir;
    (void)ext;
    (void)size;
    strlcpy(out->text, name, sizeof(out->text));
}

int main(void) {
    uint8_t ndef[NFC_QR_NDEF_MAX];
    const char* url = "https://example.com";
    assert(nfc_qr_build_ndef_message(url, ndef, sizeof(ndef)) == 16);
    assert(!memcmp(
        ndef,
        "\xD1\x01\x0C\x55\x04"
        "example.com",
        16));
    assert(nfc_qr_build_ndef_message("hello", ndef, sizeof(ndef)) == 12);
    assert(!memcmp(
        ndef,
        "\xD1\x01\x08\x54\x02"
        "enhello",
        12));
    assert(!nfc_qr_build_ndef_message("", ndef, sizeof(ndef)));
    assert(!nfc_qr_build_ndef_message(url, ndef, 4));

    const char* contact = "BEGIN:VCARD\nVERSION:3.0\nFN:Example\nEND:VCARD";
    size_t length = nfc_qr_build_ndef_message(contact, ndef, sizeof(ndef));
    assert(length == strlen(contact) + 13 && ndef[0] == 0xD2);
    assert(!memcmp(ndef + 3, "text/vcard", 10));

    const char bom_url[] = "\xEF\xBB\xBFhttps://example.com\r\n";
    put(NFC_QR_DATA_DIR "/website.TXT", bom_url, sizeof(bom_url) - 1);
    put(NFC_QR_DATA_DIR "/contact.vcf", contact, strlen(contact));
    put(NFC_QR_DATA_DIR "/ignored.ndef", "x", 1);
    put(NFC_QR_DATA_DIR "/this_name_is_far_too_long_for_a_card.txt", url, strlen(url));
    NfcQrPayload cards[16];
    assert(nfc_qr_storage_scan(&sd, cards, 16) == 2);
    assert(!strcmp(cards[0].name, "contact") && !strcmp(cards[1].name, "website"));
    char text[NFC_QR_TEXT_MAX + 1];
    assert(nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)) && !strcmp(text, url));

    NfcQrPayload saved;
    assert(nfc_qr_storage_add_payload(&sd, "Website", url, &saved));
    assert(!strcmp(saved.name, "website_1"));
    assert(nfc_qr_storage_update_payload(
        &sd, &cards[0], "BEGIN:VCARD\\nVERSION:3.0\\nFN:Changed\\nEND:VCARD"));
    assert(find(NFC_QR_DATA_DIR "/contact.txt") < 0);
    assert(
        nfc_qr_storage_read_text(&sd, &cards[0], text, sizeof(text)) &&
        strstr(text, "\nFN:Changed\n"));
    assert(nfc_qr_storage_delete_payload(&sd, &cards[0]));
    assert(find(NFC_QR_DATA_DIR "/contact.vcf") < 0);

    uint8_t boundary[NFC_QR_TEXT_MAX + 1];
    memset(boundary, 'A', sizeof(boundary));
    put(cards[1].txt_path, boundary, NFC_QR_TEXT_MAX);
    assert(nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)));
    assert(strlen(text) == NFC_QR_TEXT_MAX);
    put(cards[1].txt_path, boundary, sizeof(boundary));
    assert(!nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)));
    put(cards[1].txt_path, "a\0b", 3);
    assert(!nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)));
    put(cards[1].txt_path, "\xFF\xFEh\0", 4);
    assert(!nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)));
    put(cards[1].txt_path, "\r\n", 2);
    assert(!nfc_qr_storage_read_text(&sd, &cards[1], text, sizeof(text)));
    assert(nfc_qr_storage_scan(&sd, cards, 1) == 1);
    puts(
        "PASS: NDEF bytes, TXT/VCF discovery, BOM/CRLF, naming collisions, vCard editing, length and binary rejection");
    return 0;
}
