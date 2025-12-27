// ArduboyFX.cpp
#include "lib/ArduboyFX.h"
#include <stdlib.h>

uint16_t FX::programDataPage = 0;
uint16_t FX::programSavePage = 0;

Storage* FX::storage_ = nullptr;
File*    FX::data_ = nullptr;
File*    FX::save_ = nullptr;

bool FX::data_opened_ = false;
bool FX::save_opened_ = false;

char FX::data_path_[FX::kPathMax] = "/ext/apps_data/fxdata.bin";
char FX::save_path_[FX::kPathMax] = "/ext/apps_data/fxdata-save.bin";

FX::Domain FX::domain_ = FX::Domain::Data;
uint32_t   FX::cur_abs_ = 0;

uint32_t FX::page_size_ = 2048;
uint8_t  FX::cache_pages_ = 16;

uint8_t*  FX::cache_mem_ = nullptr;
uint32_t* FX::cache_base_ = nullptr;
uint16_t* FX::cache_len_ = nullptr;
uint32_t* FX::cache_age_ = nullptr;
uint8_t*  FX::cache_valid_ = nullptr;
uint32_t  FX::cache_age_ctr_ = 1;

uint8_t FX::last_hit_ = 0xFF;
uint32_t FX::last_base_ = 0;
uint8_t FX::seq_score_ = 0;

uint32_t FX::data_file_pos_ = 0;
bool FX::data_file_pos_valid_ = false;

uint8_t* FX::save_ram_ = nullptr;
bool FX::save_loaded_ = false;
bool FX::save_dirty_ = false;

static inline uint32_t fx_min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static inline size_t fx_min_sz(size_t a, size_t b) { return a < b ? a : b; }

uint32_t FX::alignDown_(uint32_t v, uint32_t a) { return a ? (v & ~(a - 1u)) : v; }
uint32_t FX::alignUp_(uint32_t v, uint32_t a) { return a ? ((v + (a - 1u)) & ~(a - 1u)) : v; }

void FX::setCacheConfig(uint32_t page_size, uint8_t pages) {
    if(page_size < 512) page_size = 512;
    page_size = alignUp_(page_size, 512);
    if(pages < 2) pages = 2;
    page_size_ = page_size;
    cache_pages_ = pages;
}

void FX::setPaths(const char* data_bin_path, const char* save_path) {
    if(data_bin_path && *data_bin_path) {
        strncpy(data_path_, data_bin_path, sizeof(data_path_) - 1);
        data_path_[sizeof(data_path_) - 1] = 0;
    }
    if(save_path && *save_path) {
        strncpy(save_path_, save_path, sizeof(save_path_) - 1);
        save_path_[sizeof(save_path_) - 1] = 0;
    }
}

bool FX::ensureStorage_() {
    if(storage_) return true;
    storage_ = (Storage*)furi_record_open(RECORD_STORAGE);
    if(!storage_) return false;
    data_ = storage_file_alloc(storage_);
    save_ = storage_file_alloc(storage_);
    if(!data_ || !save_) {
        end();
        return false;
    }
    return true;
}

bool FX::openData_() {
    if(data_opened_) return true;
    if(!data_) return false;
    if(!storage_file_open(data_, data_path_, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    data_opened_ = true;
    data_file_pos_valid_ = false;
    return true;
}

bool FX::fileFill_(File* f, uint8_t value, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, 0, true)) return false;
    uint8_t buf[256];
    memset(buf, value, sizeof(buf));
    size_t remain = len;
    while(remain) {
        size_t chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
        if(storage_file_write(f, buf, chunk) != chunk) return false;
        remain -= chunk;
    }
    return true;
}

bool FX::fileReadAt_(File* f, uint32_t off, void* out, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    return storage_file_read(f, out, len) == len;
}

bool FX::fileWriteAt_(File* f, uint32_t off, const void* in, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    return storage_file_write(f, in, len) == len;
}

bool FX::openSave_() {
    if(save_opened_) return true;
    if(!save_) return false;

    if(!storage_file_open(save_, save_path_, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS)) return false;
    save_opened_ = true;

    uint8_t tmp = 0;
    bool ok_tail = fileReadAt_(save_, kSaveBlockSize - 1, &tmp, 1);
    if(!ok_tail) {
        storage_file_close(save_);
        save_opened_ = false;

        if(!storage_file_open(save_, save_path_, FSAM_WRITE, FSOM_CREATE_ALWAYS)) return false;
        save_opened_ = true;
        if(!fileFill_(save_, 0xFF, kSaveBlockSize)) return false;

        storage_file_close(save_);
        save_opened_ = false;

        if(!storage_file_open(save_, save_path_, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) return false;
        save_opened_ = true;
    }

    return true;
}

void FX::freeCaches_() {
    if(cache_mem_) { free(cache_mem_); cache_mem_ = nullptr; }
    if(cache_base_) { free(cache_base_); cache_base_ = nullptr; }
    if(cache_len_) { free(cache_len_); cache_len_ = nullptr; }
    if(cache_age_) { free(cache_age_); cache_age_ = nullptr; }
    if(cache_valid_) { free(cache_valid_); cache_valid_ = nullptr; }
    if(save_ram_) { free(save_ram_); save_ram_ = nullptr; }
    save_loaded_ = false;
    save_dirty_ = false;
    last_hit_ = 0xFF;
    last_base_ = 0;
    seq_score_ = 0;
    data_file_pos_valid_ = false;
    data_file_pos_ = 0;
}

bool FX::allocCaches_() {
    freeCaches_();

    cache_mem_ = (uint8_t*)malloc((size_t)page_size_ * (size_t)cache_pages_);
    cache_base_ = (uint32_t*)malloc(sizeof(uint32_t) * cache_pages_);
    cache_len_ = (uint16_t*)malloc(sizeof(uint16_t) * cache_pages_);
    cache_age_ = (uint32_t*)malloc(sizeof(uint32_t) * cache_pages_);
    cache_valid_ = (uint8_t*)malloc(sizeof(uint8_t) * cache_pages_);
    save_ram_ = (uint8_t*)malloc(kSaveBlockSize);

    if(!cache_mem_ || !cache_base_ || !cache_len_ || !cache_age_ || !cache_valid_ || !save_ram_) {
        freeCaches_();
        return false;
    }

    for(uint8_t i = 0; i < cache_pages_; i++) {
        cache_valid_[i] = 0;
        cache_base_[i] = 0;
        cache_len_[i] = 0;
        cache_age_[i] = 0;
    }
    cache_age_ctr_ = 1;
    last_hit_ = 0xFF;
    last_base_ = 0;
    seq_score_ = 0;

    memset(save_ram_, 0xFF, kSaveBlockSize);
    save_loaded_ = false;
    save_dirty_ = false;

    data_file_pos_valid_ = false;
    data_file_pos_ = 0;

    return true;
}

uint32_t FX::absDataOffset_(uint24_t address) {
    return (uint32_t)address + ((uint32_t)programDataPage << 8);
}

void FX::saveEnsureLoaded_() {
    if(save_loaded_) return;
    if(!save_opened_ || !save_ram_) return;

    if(!fileReadAt_(save_, 0, save_ram_, kSaveBlockSize)) {
        memset(save_ram_, 0xFF, kSaveBlockSize);
    }
    save_loaded_ = true;
    save_dirty_ = false;
}

void FX::commitSave() {
    if(!save_opened_) return;
    saveEnsureLoaded_();
    if(!save_dirty_) return;
    (void)fileWriteAt_(save_, 0, save_ram_, kSaveBlockSize);
    save_dirty_ = false;
}

bool FX::begin() {
    if(!ensureStorage_()) return false;
    if(!openData_()) return false;
    if(!openSave_()) return false;
    if(!allocCaches_()) return false;

    domain_ = Domain::Data;
    cur_abs_ = 0;

    saveEnsureLoaded_();

    return true;
}

bool FX::begin(uint16_t developmentDataPage) {
    programDataPage = developmentDataPage;
    return begin();
}

bool FX::begin(uint16_t developmentDataPage, uint16_t developmentSavePage) {
    programDataPage = developmentDataPage;
    programSavePage = developmentSavePage;
    return begin();
}

void FX::end() {
    if(data_) {
        storage_file_close(data_);
        storage_file_free(data_);
        data_ = nullptr;
    }
    if(save_) {
        storage_file_close(save_);
        storage_file_free(save_);
        save_ = nullptr;
    }
    data_opened_ = false;
    save_opened_ = false;

    if(storage_) {
        furi_record_close(RECORD_STORAGE);
        storage_ = nullptr;
    }

    freeCaches_();
}

void FX::readJedecID(JedecID& id) { id.manufacturer = 0; id.device = 0; id.size = 0; }
void FX::readJedecID(JedecID* id) { if(id) readJedecID(*id); }
bool FX::detect() { uint8_t b; return data_opened_ ? fileReadAt_(data_, 0, &b, 1) : false; }
void FX::noFXReboot() {}

uint8_t FX::writeByte(uint8_t data) { return data; }
uint8_t FX::readByte() { return 0; }
void FX::writeCommand(uint8_t) {}
void FX::wakeUp() {}
void FX::sleep() {}
void FX::writeEnable() {}
void FX::seekCommand(uint8_t, uint24_t) {}

bool FX::dataPageHas_(uint32_t base) {
    if(last_hit_ != 0xFF && cache_valid_[last_hit_] && cache_base_[last_hit_] == base) return true;
    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(cache_valid_[i] && cache_base_[i] == base) { last_hit_ = i; return true; }
    }
    return false;
}

uint8_t FX::dataPickVictim_() {
    uint8_t victim = 0;
    uint32_t best_age = 0xFFFFFFFFu;
    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(!cache_valid_[i]) return i;
        uint32_t a = cache_age_[i];
        if(a < best_age) { best_age = a; victim = i; }
    }
    return victim;
}

bool FX::dataLoadPage_(uint32_t base, uint8_t page_i) {
    if(!data_opened_ && !openData_()) return false;
    if(!data_) return false;

    if(!data_file_pos_valid_ || data_file_pos_ != base) {
        if(!storage_file_seek(data_, base, true)) return false;
        data_file_pos_ = base;
        data_file_pos_valid_ = true;
    }

    uint8_t* dst = cache_mem_ + ((size_t)page_i * (size_t)page_size_);
    size_t r = storage_file_read(data_, dst, page_size_);
    if(r == 0) return false;

    data_file_pos_ += (uint32_t)r;

    cache_valid_[page_i] = 1;
    cache_base_[page_i] = base;
    cache_len_[page_i] = (uint16_t)r;
    cache_age_[page_i] = cache_age_ctr_++;
    last_hit_ = page_i;

    return true;
}

void FX::dataMaybePrefetch_(uint32_t base) {
    if(cache_pages_ < 2) return;
    if(seq_score_ < 2) return;

    uint32_t next_base = base + page_size_;
    if(dataPageHas_(next_base)) return;

    uint8_t victim = dataPickVictim_();
    if(cache_valid_[victim] && cache_base_[victim] == base) return;

    (void)dataLoadPage_(next_base, victim);
}

bool FX::dataEnsurePageIndex_(uint32_t abs_off, uint8_t* out_index) {
    if(!cache_mem_ || !out_index) return false;

    uint32_t base = alignDown_(abs_off, page_size_);

    if(last_hit_ != 0xFF && cache_valid_[last_hit_] && cache_base_[last_hit_] == base) {
        cache_age_[last_hit_] = cache_age_ctr_++;
        *out_index = last_hit_;
        if(base == last_base_ + page_size_) { if(seq_score_ < 255) seq_score_++; } else seq_score_ = 0;
        last_base_ = base;
        dataMaybePrefetch_(base);
        return true;
    }

    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(cache_valid_[i] && cache_base_[i] == base) {
            cache_age_[i] = cache_age_ctr_++;
            last_hit_ = i;
            *out_index = i;
            if(base == last_base_ + page_size_) { if(seq_score_ < 255) seq_score_++; } else seq_score_ = 0;
            last_base_ = base;
            dataMaybePrefetch_(base);
            return true;
        }
    }

    uint8_t victim = dataPickVictim_();
    if(!dataLoadPage_(base, victim)) return false;

    *out_index = victim;

    if(base == last_base_ + page_size_) { if(seq_score_ < 255) seq_score_++; } else seq_score_ = 0;
    last_base_ = base;
    dataMaybePrefetch_(base);

    return true;
}

bool FX::dataReadSpanCached_(uint32_t abs_off, uint8_t* out, size_t len) {
    if(!out || len == 0) return true;

    while(len) {
        uint8_t page_i = 0xFF;
        if(!dataEnsurePageIndex_(abs_off, &page_i)) return false;

        uint32_t base = cache_base_[page_i];
        uint32_t idx  = abs_off - base;

        uint16_t plen = cache_len_[page_i];
        if(idx >= plen) return false;

        size_t avail = (size_t)plen - (size_t)idx;
        size_t take = fx_min_sz(len, avail);

        const uint8_t* src = cache_mem_ + ((size_t)page_i * (size_t)page_size_) + idx;
        memcpy(out, src, take);

        out += take;
        abs_off += (uint32_t)take;
        len -= take;
    }
    return true;
}

void FX::seekData(uint24_t address) {
    domain_ = Domain::Data;
    cur_abs_ = absDataOffset_(address);
}

void FX::seekDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize) {
    uint32_t add = (elementSize == 0) ? (uint32_t)index * 256u : (uint32_t)index * (uint32_t)elementSize;
    add += offset;
    seekData(address + add);
}

void FX::seekSave(uint24_t address) {
    domain_ = Domain::Save;
    cur_abs_ = (uint32_t)address;
    saveEnsureLoaded_();
}

uint8_t FX::readPendingUInt8() {
    if(domain_ == Domain::Data) {
        uint8_t v = 0xFF;
        (void)dataReadSpanCached_(cur_abs_, &v, 1);
        cur_abs_++;
        return v;
    }

    saveEnsureLoaded_();
    uint8_t v = 0xFF;
    if(save_ram_ && cur_abs_ < kSaveBlockSize) v = save_ram_[cur_abs_];
    cur_abs_++;
    return v;
}

uint8_t FX::readPendingLastUInt8() { return readEnd(); }

uint16_t FX::readPendingUInt16() {
    uint16_t hi = readPendingUInt8();
    uint16_t lo = readPendingUInt8();
    return (hi << 8) | lo;
}

uint16_t FX::readPendingLastUInt16() { return readPendingUInt16(); }

uint24_t FX::readPendingUInt24() {
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b2 << 16) | (b1 << 8) | b0;
}

uint24_t FX::readPendingLastUInt24() { return readPendingUInt24(); }

uint32_t FX::readPendingUInt32() {
    uint32_t b3 = readPendingUInt8();
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

uint32_t FX::readPendingLastUInt32() { return readPendingUInt32(); }

void FX::readBytes(uint8_t* buffer, size_t length) {
    if(!buffer || length == 0) return;

    if(domain_ == Domain::Data) {
        (void)dataReadSpanCached_(cur_abs_, buffer, length);
        cur_abs_ += (uint32_t)length;
        return;
    }

    saveEnsureLoaded_();
    size_t i = 0;
    while(i < length) {
        uint32_t off = cur_abs_++;
        buffer[i++] = (save_ram_ && off < kSaveBlockSize) ? save_ram_[off] : 0xFF;
    }
}

void FX::readBytesEnd(uint8_t* buffer, size_t length) { readBytes(buffer, length); }

uint8_t FX::readEnd() { return readPendingUInt8(); }

void FX::readDataBytes(uint24_t address, uint8_t* buffer, size_t length) {
    seekData(address);
    readBytesEnd(buffer, length);
}

void FX::readSaveBytes(uint24_t address, uint8_t* buffer, size_t length) {
    seekSave(address);
    readBytesEnd(buffer, length);
}

void FX::readDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize,
                       uint8_t* buffer, size_t length) {
    seekDataArray(address, index, offset, elementSize);
    readBytesEnd(buffer, length);
}

uint8_t FX::readIndexedUInt8(uint24_t address, uint8_t index) {
    seekDataArray(address, index, 0, sizeof(uint8_t));
    return readEnd();
}

uint16_t FX::readIndexedUInt16(uint24_t address, uint8_t index) {
    seekDataArray(address, index, 0, sizeof(uint16_t));
    return readPendingLastUInt16();
}

uint24_t FX::readIndexedUInt24(uint24_t address, uint8_t index) {
    seekDataArray(address, index, 0, 3);
    return readPendingLastUInt24();
}

uint32_t FX::readIndexedUInt32(uint24_t address, uint8_t index) {
    seekDataArray(address, index, 0, 4);
    return readPendingLastUInt32();
}

uint16_t FX::readSaveU16BE_(uint16_t off) {
    saveEnsureLoaded_();
    if(!save_ram_ || off + 1 >= kSaveBlockSize) return 0xFFFF;
    return ((uint16_t)save_ram_[off] << 8) | save_ram_[off + 1];
}

void FX::writeSaveU16BE_(uint16_t off, uint16_t v) {
    saveEnsureLoaded_();
    if(!save_ram_ || off + 1 >= kSaveBlockSize) return;
    save_ram_[off] = (uint8_t)(v >> 8);
    save_ram_[off + 1] = (uint8_t)(v & 0xFF);
    save_dirty_ = true;
}

void FX::waitWhileBusy() {}

void FX::eraseSaveBlock(uint16_t) {
    saveEnsureLoaded_();
    if(!save_ram_) return;
    memset(save_ram_, 0xFF, kSaveBlockSize);
    save_dirty_ = true;
}

uint8_t FX::loadGameState(uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return 0;
    saveEnsureLoaded_();
    if(!save_ram_) return 0;

    uint16_t addr = 0;
    uint8_t loaded = 0;

    while(true) {
        if(addr + 2 > kSaveBlockSize) break;
        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > kSaveBlockSize) break;

        memcpy(gameState, &save_ram_[addr + 2], size);
        loaded = 1;
        addr = (uint16_t)next;
    }

    return loaded;
}

void FX::saveGameState(const uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return;
    saveEnsureLoaded_();
    if(!save_ram_) return;

    uint16_t addr = 0;

    while(true) {
        if(addr + 2 > kSaveBlockSize) { addr = 0; break; }
        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > kSaveBlockSize) { addr = 0; break; }
        addr = (uint16_t)next;
    }

    if(((uint32_t)addr + 2u + (uint32_t)size) > 4094u) {
        eraseSaveBlock(0);
        waitWhileBusy();
        addr = 0;
    }

    writeSaveU16BE_(addr, (uint16_t)size);
    memcpy(&save_ram_[addr + 2], gameState, size);
    save_dirty_ = true;
}

void FX::warmUpData(uint24_t address, size_t length) {
    if(!data_opened_ && !openData_()) return;
    if(page_size_ == 0) return;

    uint32_t abs = absDataOffset_(address);
    uint32_t end = abs + (uint32_t)length;
    uint32_t p = alignDown_(abs, page_size_);

    while(p < end) {
        uint8_t page_i = 0xFF;
        (void)dataEnsurePageIndex_(p, &page_i);
        p += page_size_;
    }
}
