// ArduboyFX.cpp
#include "lib/ArduboyFX.h"

uint16_t FX::programDataPage = 0;
uint16_t FX::programSavePage = 0;

Storage* FX::storage_ = nullptr;
File*    FX::data_ = nullptr;
File*    FX::save_ = nullptr;

bool FX::data_opened_ = false;
bool FX::save_opened_ = false;

char FX::data_path_[128] = "/ext/fxdata.bin";
char FX::save_path_[128] = "/ext/fxdata-save.bin";

FX::Domain FX::domain_ = FX::Domain::Data;
uint32_t   FX::cur_abs_ = 0;

uint8_t  FX::io_buf_[FX::kIOBufSize];
uint16_t FX::io_pos_ = 0;
uint16_t FX::io_len_ = 0;

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
    return true;
}

bool FX::fileFill_(File* f, uint8_t value, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, 0, true)) return false;
    uint8_t buf[64];
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
    }

    return true;
}

void FX::closeFiles_() {
    if(data_) storage_file_close(data_);
    if(save_) storage_file_close(save_);
    data_opened_ = false;
    save_opened_ = false;
}

bool FX::begin() {
    if(!ensureStorage_()) return false;
    if(!openData_()) return false;
    if(!openSave_()) return false;
    domain_ = Domain::Data;
    cur_abs_ = 0;
    io_pos_ = 0;
    io_len_ = 0;
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
}

uint8_t FX::writeByte(uint8_t data) { return data; }
uint8_t FX::readByte() { return 0; }
void FX::writeCommand(uint8_t) {}
void FX::wakeUp() {}
void FX::sleep() {}
void FX::writeEnable() {}
void FX::seekCommand(uint8_t, uint24_t) {}

void FX::readJedecID(JedecID& id) {
    id.manufacturer = 0;
    id.device = 0;
    id.size = 0;
}

void FX::readJedecID(JedecID* id) {
    if(id) readJedecID(*id);
}

bool FX::detect() {
    if(!data_opened_ && !openData_()) return false;
    uint8_t b;
    return fileReadAt_(data_, 0, &b, 1);
}

void FX::noFXReboot() {}

uint32_t FX::absDataOffset_(uint24_t address) {
    return (uint32_t)address + ((uint32_t)programDataPage << 8);
}

void FX::seekData(uint24_t address) {
    domain_ = Domain::Data;
    cur_abs_ = absDataOffset_(address);
    io_pos_ = 0;
    io_len_ = 0;
    storage_file_seek(data_, cur_abs_, true);
}

void FX::seekDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize) {
    uint32_t add = (elementSize == 0) ? (uint32_t)index * 256u : (uint32_t)index * (uint32_t)elementSize;
    add += offset;
    seekData(address + add);
}

void FX::seekSave(uint24_t address) {
    domain_ = Domain::Save;
    cur_abs_ = (uint32_t)address;
    io_pos_ = 0;
    io_len_ = 0;
    storage_file_seek(save_, cur_abs_, true);
}

bool FX::ioRefill_() {
    File* f = (domain_ == Domain::Data) ? data_ : save_;
    if(!f) return false;

    size_t want = kIOBufSize;
    if(domain_ == Domain::Save) {
        if(cur_abs_ >= kSaveBlockSize) return false;
        uint32_t remain = kSaveBlockSize - cur_abs_;
        if(remain < want) want = remain;
    }

    io_len_ = (uint16_t)storage_file_read(f, io_buf_, want);
    io_pos_ = 0;
    return io_len_ != 0;
}

uint8_t FX::readPendingUInt8() {
    if(domain_ == Domain::Save && cur_abs_ >= kSaveBlockSize) {
        cur_abs_++;
        return 0xFF;
    }

    if(io_pos_ >= io_len_) {
        if(!ioRefill_()) {
            cur_abs_++;
            return 0xFF;
        }
    }

    uint8_t v = io_buf_[io_pos_++];
    cur_abs_++;
    return v;
}

uint8_t FX::readPendingLastUInt8() {
    return readEnd();
}

uint16_t FX::readPendingUInt16() {
    uint16_t hi = readPendingUInt8();
    uint16_t lo = readPendingUInt8();
    return (hi << 8) | lo;
}

uint16_t FX::readPendingLastUInt16() {
    return readPendingUInt16();
}

uint24_t FX::readPendingUInt24() {
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b2 << 16) | (b1 << 8) | b0;
}

uint24_t FX::readPendingLastUInt24() {
    return readPendingUInt24();
}

uint32_t FX::readPendingUInt32() {
    uint32_t b3 = readPendingUInt8();
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

uint32_t FX::readPendingLastUInt32() {
    return readPendingUInt32();
}

void FX::readBytes(uint8_t* buffer, size_t length) {
    for(size_t i = 0; i < length; i++) buffer[i] = readPendingUInt8();
}

void FX::readBytesEnd(uint8_t* buffer, size_t length) {
    if(length == 0) return;
    for(size_t i = 0; i + 1 < length; i++) buffer[i] = readPendingUInt8();
    buffer[length - 1] = readEnd();
}

uint8_t FX::readEnd() {
    return readPendingUInt8();
}

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
    uint8_t b[2] = {0xFF, 0xFF};
    if(off + 1 < kSaveBlockSize) (void)fileReadAt_(save_, off, b, 2);
    return ((uint16_t)b[0] << 8) | b[1];
}

void FX::writeSaveU16BE_(uint16_t off, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
    if(off + 1 < kSaveBlockSize) (void)fileWriteAt_(save_, off, b, 2);
}

void FX::waitWhileBusy() {}

void FX::eraseSaveBlock(uint16_t) {
    (void)fileFill_(save_, 0xFF, kSaveBlockSize);
}

uint8_t FX::loadGameState(uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return 0;

    uint16_t addr = 0;
    uint8_t loaded = 0;

    while(true) {
        if(addr + 2 > kSaveBlockSize) break;

        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > kSaveBlockSize) break;

        (void)fileReadAt_(save_, addr + 2, gameState, size);
        loaded = 1;
        addr = (uint16_t)next;
    }

    return loaded;
}

void FX::saveGameState(const uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return;

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
    (void)fileWriteAt_(save_, addr + 2, gameState, size);
}
