#include "lib/ArduboyFX.h"

// ---------------- static storage ----------------
uint16_t FX::programDataPage = 0;
uint16_t FX::programSavePage = 0;

Storage* FX::storage_ = nullptr;
File*    FX::data_ = nullptr;
File*    FX::save_ = nullptr;

bool FX::data_opened_ = false;
bool FX::save_opened_ = false;

char FX::data_path_[128] = "/ext/fxdata-data.bin";
char FX::save_path_[128] = "/ext/fxdata-ave.bin";

FX::Domain FX::domain_ = FX::Domain::Data;
uint32_t   FX::cur_abs_ = 0;

// ---------------- path config ----------------
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

// ---------------- lifecycle ----------------
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

    if(!storage_file_open(data_, data_path_, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return false;
    }
    data_opened_ = true;
    return true;
}

bool FX::openSave_() {
    if(save_opened_) return true;
    if(!save_) return false;

    // RW, создать если нет
    if(!storage_file_open(save_, save_path_, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS)) {
        return false;
    }
    save_opened_ = true;

    // Гарантируем "erase-sector" размер 4096 и заполнение 0xFF.
    // Самый надёжный вариант: если файл пустой или не 4096 — пересоздать.
    // Проверка size в разных SDK может отличаться, поэтому делаем консервативно:
    // читаем 1 байт в конце — если не выходит, пересоздадим.
    uint8_t tmp = 0;
    bool ok_tail = fileRead_(save_, kSaveBlockSize - 1, &tmp, 1);

    if(!ok_tail) {
        storage_file_close(save_);
        save_opened_ = false;

        if(!storage_file_open(save_, save_path_, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            return false;
        }
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

// ---------------- compat stubs ----------------
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
    // Опциональная сигнатура как в исходнике (0x4152 = 'A''R')
    uint8_t b[2];
    if(!fileRead_(data_, 0, b, 2)) return false;
    uint16_t v = ((uint16_t)b[0] << 8) | b[1];
    return v == 0x4152;
}

void FX::noFXReboot() {
    // На Flipper bootloader не нужен — оставляем заглушкой
}

// ---------------- file helpers ----------------
uint32_t FX::absDataOffset_(uint24_t address) {
    return (uint32_t)address + ((uint32_t)programDataPage << 8);
}

bool FX::fileRead_(File* f, uint32_t off, void* out, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    size_t r = storage_file_read(f, out, len);
    return r == len;
}

bool FX::fileWrite_(File* f, uint32_t off, const void* in, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    size_t w = storage_file_write(f, in, len);
    return w == len;
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

// ---------------- seek ----------------
void FX::seekData(uint24_t address) {
    domain_ = Domain::Data;
    cur_abs_ = absDataOffset_(address);
}

void FX::seekDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize) {
    // как в оригинале: size 0 => 256
    uint32_t add = (elementSize == 0) ? (uint32_t)index * 256u : (uint32_t)index * (uint32_t)elementSize;
    add += offset;
    seekData(address + add);
}

void FX::seekSave(uint24_t address) {
    domain_ = Domain::Save;
    cur_abs_ = (uint32_t)address; // offset внутри 4KB save блока
}

// ---------------- pending read core ----------------
uint8_t FX::readPendingUInt8() {
    uint8_t v = 0xFF;

    if(domain_ == Domain::Data) {
        (void)fileRead_(data_, cur_abs_, &v, 1);
        cur_abs_++;
        return v;
    } else {
        // Save: читаем из 4KB файла
        if(cur_abs_ < kSaveBlockSize) {
            (void)fileRead_(save_, cur_abs_, &v, 1);
        } else {
            v = 0xFF;
        }
        cur_abs_++;
        return v;
    }
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
    // На Flipper "end transaction" не нужен — просто читаем
    return readPendingUInt8();
}

// ---------------- convenience data/save bytes ----------------
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

// ---------------- indexed reads ----------------
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

// ---------------- save block helpers ----------------
uint16_t FX::readSaveU16BE_(uint16_t off) {
    uint8_t b[2] = {0xFF, 0xFF};
    if(off + 1 < kSaveBlockSize) {
        (void)fileRead_(save_, off, b, 2);
    }
    return ((uint16_t)b[0] << 8) | b[1];
}

void FX::writeSaveU16BE_(uint16_t off, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
    if(off + 1 < kSaveBlockSize) {
        (void)fileWrite_(save_, off, b, 2);
    }
}

void FX::waitWhileBusy() {
    // no-op
}

void FX::eraseSaveBlock(uint16_t /*page*/) {
    (void)fileFill_(save_, 0xFF, kSaveBlockSize);
}

// ---------------- GameState (как ArduboyFX по логике) ----------------
uint8_t FX::loadGameState(uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return 0;

    uint16_t addr = 0;
    uint8_t loaded = 0;

    // Читаем "самый последний валидный" блок того же size
    while(true) {
        if(addr + 2 > kSaveBlockSize) break;

        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > kSaveBlockSize) break;

        // читаем payload
        (void)fileRead_(save_, addr + 2, gameState, size);
        loaded = 1;
        addr = (uint16_t)next;
    }

    return loaded;
}

void FX::saveGameState(const uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return;

    // найти конец цепочки предыдущих сейвов (того же размера)
    uint16_t addr = 0;

    while(true) {
        if(addr + 2 > kSaveBlockSize) { addr = 0; break; }

        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > kSaveBlockSize) { addr = 0; break; }

        addr = (uint16_t)next;
    }

    // как в оригинале: последние 2 байта 4KB всегда "не используем"
    // => доступно максимум 4094 байта под записи
    if(((uint32_t)addr + 2u + (uint32_t)size) > 4094u) {
        eraseSaveBlock(0);
        waitWhileBusy();
        addr = 0;
    }

    // write header + data
    writeSaveU16BE_(addr, (uint16_t)size);
    (void)fileWrite_(save_, addr + 2, gameState, size);

    // при желании можно storage_file_sync(save_) если в твоём SDK доступно
}
