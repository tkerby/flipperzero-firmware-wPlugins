// ArduboyFX.h
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <furi.h>
#include <storage/storage.h>

using uint24_t = uint32_t;

struct JedecID {
    uint8_t manufacturer;
    uint8_t device;
    uint8_t size;
};

class FX {
public:
    static uint16_t programDataPage;
    static uint16_t programSavePage;

    static void setPaths(const char* data_bin_path, const char* save_path);

    static bool begin();
    static bool begin(uint16_t developmentDataPage);
    static bool begin(uint16_t developmentDataPage, uint16_t developmentSavePage);
    static void end();

    static void readJedecID(JedecID& id);
    static void readJedecID(JedecID* id);
    static bool detect();
    static void noFXReboot();

    static void seekData(uint24_t address);
    static void seekDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize);
    static void seekSave(uint24_t address);

    static uint8_t  readPendingUInt8();
    static uint8_t  readPendingLastUInt8();

    static uint16_t readPendingUInt16();
    static uint16_t readPendingLastUInt16();

    static uint24_t readPendingUInt24();
    static uint24_t readPendingLastUInt24();

    static uint32_t readPendingUInt32();
    static uint32_t readPendingLastUInt32();

    static void readBytes(uint8_t* buffer, size_t length);
    static void readBytesEnd(uint8_t* buffer, size_t length);

    static uint8_t readEnd();

    static void readDataBytes(uint24_t address, uint8_t* buffer, size_t length);
    static void readSaveBytes(uint24_t address, uint8_t* buffer, size_t length);

    static void readDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize,
                              uint8_t* buffer, size_t length);

    static uint8_t  readIndexedUInt8(uint24_t address, uint8_t index);
    static uint16_t readIndexedUInt16(uint24_t address, uint8_t index);
    static uint24_t readIndexedUInt24(uint24_t address, uint8_t index);
    static uint32_t readIndexedUInt32(uint24_t address, uint8_t index);

    static uint8_t loadGameState(uint8_t* gameState, size_t size);
    static void    saveGameState(const uint8_t* gameState, size_t size);

    static void eraseSaveBlock(uint16_t page = 0);
    static void waitWhileBusy();

    static uint8_t writeByte(uint8_t data);
    static uint8_t readByte();
    static void writeCommand(uint8_t command);
    static void wakeUp();
    static void sleep();
    static void writeEnable();
    static void seekCommand(uint8_t command, uint24_t address);

private:
    enum class Domain : uint8_t { Data, Save };

    static bool ensureStorage_();
    static bool openData_();
    static bool openSave_();
    static void closeFiles_();

    static uint32_t absDataOffset_(uint24_t address);

    static bool fileReadAt_(File* f, uint32_t off, void* out, size_t len);
    static bool fileWriteAt_(File* f, uint32_t off, const void* in, size_t len);
    static bool fileFill_(File* f, uint8_t value, size_t len);

    static bool ioRefill_();

    static uint16_t readSaveU16BE_(uint16_t off);
    static void     writeSaveU16BE_(uint16_t off, uint16_t v);

private:
    static constexpr uint16_t kSaveBlockSize = 4096;
    static constexpr size_t   kIOBufSize = 512;

    static Storage* storage_;
    static File* data_;
    static File* save_;
    static bool data_opened_;
    static bool save_opened_;

    static char data_path_[128];
    static char save_path_[128];

    static Domain domain_;
    static uint32_t cur_abs_;

    static uint8_t  io_buf_[kIOBufSize];
    static uint16_t io_pos_;
    static uint16_t io_len_;
};
