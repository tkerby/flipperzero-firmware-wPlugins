#pragma once

#include <lib/subghz/protocols/base.h>

#include "SubGhzReceivedData.hpp"

class SubGhzReceivedDataStub : public SubGhzReceivedData {
private:
    const char* protocolName;
    uint32_t hash;

public:
    SubGhzReceivedDataStub(const char* protocolName, uint32_t hash) {
        this->protocolName = protocolName;
        this->hash = hash;
    }

    const char* GetProtocolName() {
        return protocolName;
    }

    uint32_t GetHash() {
        return hash;
    }

    int GetTE() {
        return 212;
    }
};
