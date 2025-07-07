#include <storage/storage.h>

void save_result(const char* text, char* file_name) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "/ext/flip_crypt_saved/%s.txt", file_name);
    
    if(storage_simply_mkdir(furi_record_open(RECORD_STORAGE), "/ext/flip_crypt_saved")) {
        if (storage_file_open(file, buffer, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, text, strlen(text));
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_aes_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("aes.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_aes_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("aes_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_affine_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("affine.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_affine_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("affine_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_atbash_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("atbash.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_atbash_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("atbash_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_baconian_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("baconian.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_baconian_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("baconian_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_beaufort_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("beaufort.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_beaufort_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("beaufort_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_caesar_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("caesar.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_caesar_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("caesar_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_playfair_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("playfair.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_playfair_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("playfair_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_polybius_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("polybius.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_polybius_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("polybius_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_railfence_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("railfence.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_railfence_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("railfence_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_rc4_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("rc4.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_rc4_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("rc4_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_rot13_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("rot13.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_rot13_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("rot13_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_scytale_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("scytale.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_scytale_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("scytale_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_vigenere_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("vigenere.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_vigenere_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("vigenere_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_blake2_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("blake2.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_fnv1a_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("fnv1a.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_md5_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("md5.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_murmur3_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("murmur3.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sip_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sip.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sha1_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sha1.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sha224_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sha224.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sha256_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sha256.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sha384_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sha384.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_sha512_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("sha512.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_xx_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("xx.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base32_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base32.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base32_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base32_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base58_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base58.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base58_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base58_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base64_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base64.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void save_base64_decrypt_result(const char* text) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, APP_DATA_PATH("base64_decrypt.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

char* load_aes() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("aes.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_aes_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("aes_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_affine() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("affine.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_affine_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("affine_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_atbash() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("atbash.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_atbash_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("atbash_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_baconian() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("baconian.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_baconian_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("baconian_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_beaufort() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("beaufort.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_beaufort_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("beaufort_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_caesar() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("caesar.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_caesar_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("caesar_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_playfair() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("playfair.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_playfair_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("playfair_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_polybius() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("polybius.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_polybius_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("polybius_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_railfence() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("railfence.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_railfence_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("railfence_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_rc4() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("rc4.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_rc4_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("rc4_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_rot13() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("rot13.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_rot13_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("rot13_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_scytale() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("scytale.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_scytale_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("scytale_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_vigenere() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("vigenere.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_vigenere_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("vigenere_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_blake2() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("blake2.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_fnv1a() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("fnv1a.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_md5() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("md5.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_murmur3() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("murmur3.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sip() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sip.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sha1() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sha1.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sha224() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sha224.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sha256() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sha256.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sha384() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sha384.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_sha512() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("sha512.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_xx() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("xx.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base32() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base32.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base32_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base32_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base58() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base58.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base58_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base58_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base64() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base64.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

char* load_base64_decrypt() {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    char* result = NULL;
    if(storage_file_open(file, APP_DATA_PATH("base64_decrypt.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        if(file_size > 0) {
            result = malloc(file_size + 1);
            storage_file_read(file, result, file_size);
            result[file_size] = '\0';
        } else {
            return "FAILURE";
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}