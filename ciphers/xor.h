uint8_t* xor_encrypt_and_decrypt(const char* input, const char* key, size_t* out_len);
char* bytes_to_hex(const uint8_t* data, size_t len, char* hex_buf, size_t hex_buf_size);