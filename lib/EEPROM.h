#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// EEPROM совместимость: RAM-эмуляция
class EEPROMClass {
public:
    // Обычно для Arduboy хватает 512-1024 байт
    static constexpr int kSize = 1024;

    EEPROMClass() = default;

    // Сколько байт доступно
    int length() const { return kSize; }

    // Прочитать байт
    uint8_t read(int addr) const {
        if(addr < 0 || addr >= kSize) return 0;
        return mem_[addr];
    }

    // Записать байт (без commit)
    void write(int addr, uint8_t value) {
        if(addr < 0 || addr >= kSize) return;
        mem_[addr] = value;
        dirty_ = true;
    }

    // Arduino EEPROM.update: пишет только если отличается
    void update(int addr, uint8_t value) {
        if(addr < 0 || addr >= kSize) return;
        if(mem_[addr] != value) {
            mem_[addr] = value;
            dirty_ = true;
        }
    }

    // Прочитать структуру/тип
    template<typename T>
    T& get(int addr, T& out) const {
        if(addr < 0 || addr + (int)sizeof(T) > kSize) {
            // если вышли за границы — просто оставим out как есть
            return out;
        }
        memcpy(&out, mem_ + addr, sizeof(T));
        return out;
    }

    // Записать структуру/тип (и commit внутри НЕ делает — как в Arduino обычно)
    template<typename T>
    const T& put(int addr, const T& in) {
        if(addr < 0 || addr + (int)sizeof(T) > kSize) {
            return in;
        }

        // put обычно пишет байты и ставит dirty только если реально поменялось
        bool changed = false;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&in);
        for(size_t i = 0; i < sizeof(T); i++) {
            const int a = addr + (int)i;
            if(mem_[a] != src[i]) {
                mem_[a] = src[i];
                changed = true;
            }
        }
        if(changed) dirty_ = true;
        return in;
    }

    // Очистить (удобно для дебага)
    void clear(uint8_t value = 0) {
        memset(mem_, value, sizeof(mem_));
        dirty_ = true;
    }

    // “Сохранить”. В RAM-only версии — просто сбрасываем dirty.
    // Если хочешь реальное сохранение на Flipper (файл в appdata),
    // сюда вставим Storage API.
    void commit() {
        dirty_ = false;
    }

    bool isDirty() const { return dirty_; }

private:
    uint8_t mem_[kSize] = {0};
    bool dirty_ = false;
};

// Глобальный объект как на Arduino
static EEPROMClass EEPROM;
