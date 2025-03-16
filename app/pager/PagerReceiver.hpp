#pragma once

#include "core/log.h"
#include <cstring>
#include <vector>

#include "app/AppConfig.hpp"

#include "lib/hardware/subghz/data/SubGhzReceivedData.hpp"
#include "ReceivedPagerData.hpp"

#include "protocol/PrincetonProtocol.hpp"
#include "protocol/Smc5326Protocol.hpp"

#include "decoder/Td157Decoder.hpp"
#include "decoder/Td165Decoder.hpp"
#include "decoder/Td174Decoder.hpp"
#include "decoder/L8RDecoder.hpp"
#include "decoder/L8SDecoder.hpp"

#undef LOG_TAG
#define LOG_TAG "PGR_RCV"

#define MAX_REPEATS 99

using namespace std;

class PagerReceiver {
public:
    vector<PagerProtocol*> protocols = {new PrincetonProtocol(), new Smc5326Protocol()};
    vector<PagerDecoder*> decoders = {
        new Td157Decoder(),
        new Td165Decoder(),
        new Td174Decoder(),
        new L8RDecoder(),
        new L8SDecoder(),
    };

private:
    AppConfig* config;
    SubGhzSettings* subghzSettings;
    vector<PagerDataStored> pagers;

    PagerProtocol* getProtocol(const char* systemProtocolName) {
        for(size_t i = 0; i < protocols.size(); i++) {
            if(strcmp(systemProtocolName, protocols[i]->GetSystemName()) == 0) {
                return protocols[i];
            }
        }
        return NULL;
    }

    PagerDecoder* getDecoder(PagerDataStored* pagerData) {
        for(size_t i = 0; i < decoders.size(); i++) {
            if(decoders[i]->GetPager(pagerData->data) <= config->MaxPagerForBatchOrDetection) {
                return decoders[i];
            }
        }
        return decoders[0];
    }

public:
    PagerReceiver(AppConfig* config, SubGhzSettings* subghzSettings) {
        this->config = config;
        this->subghzSettings = subghzSettings;

        for(size_t i = 0; i < protocols.size(); i++) {
            protocols[i]->id = i;
        }
        for(size_t i = 0; i < decoders.size(); i++) {
            decoders[i]->id = i;
        }
    }

    PagerDataGetter PagerGetter(size_t index) {
        return [this, index]() { return &pagers[index]; };
    }

    ReceivedPagerData* Receive(SubGhzReceivedData* data) {
        PagerProtocol* protocol = getProtocol(data->GetProtocolName());
        if(protocol == NULL) {
            FURI_LOG_I(LOG_TAG, "Skipping received data with unsupported protocol: %s", data->GetProtocolName());
            return NULL;
        }

        int index = -1;
        uint32_t dataHash = data->GetHash();

        for(size_t i = 0; i < pagers.size(); i++) {
            if(pagers[i].data == dataHash && pagers[i].protocol == protocol->id) {
                if(pagers[i].repeats < MAX_REPEATS) {
                    pagers[i].repeats++;
                } else {
                    return NULL; // no need to modify element any more
                }
                index = i;
                break;
            }
        }

        bool isNew = index < 0;
        if(isNew) {
            PagerDataStored storedData = PagerDataStored();
            storedData.data = dataHash;
            storedData.protocol = protocol->id;
            storedData.repeats = 1;
            storedData.te = data->GetTE();
            storedData.frequency = subghzSettings->GetFrequencyIndex(data->GetFrequency());
            storedData.decoder = getDecoder(&storedData)->id;

            index = pagers.size();
            pagers.push_back(storedData);
        }

        return new ReceivedPagerData(PagerGetter(index), index, isNew);
    }

    ~PagerReceiver() {
        for(PagerProtocol* protocol : protocols) {
            delete protocol;
        }

        for(PagerDecoder* decoder : decoders) {
            delete decoder;
        }
    }
};
