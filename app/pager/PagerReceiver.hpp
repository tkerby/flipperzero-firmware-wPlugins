#pragma once

#include "PagerSerializer.hpp"
#include <cstring>
#include <vector>
#include <map>

#include "core/log.h"

#include "lib/hardware/subghz/SubGhzSettings.hpp"
#include "lib/hardware/subghz/data/SubGhzReceivedData.hpp"

#include "app/AppConfig.hpp"

#include "data/ReceivedPagerData.hpp"
#include "data/KnownStationData.hpp"

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
    vector<StoredPagerData> pagers;
    map<uint32_t, String*> knownNames;

    PagerDecoder* getDecoder(StoredPagerData* pagerData) {
        for(size_t i = 0; i < decoders.size(); i++) {
            if(decoders[i]->GetPager(pagerData->data) <= config->MaxPagerForBatchOrDetection) {
                return decoders[i];
            }
        }
        return decoders[0];
    }

    void loadKnownStations() {
        FileManager fileManager = FileManager();
        Directory* dir = fileManager.OpenDirectory(SAVED_STATIONS_PATH);
        PagerSerializer serializer = PagerSerializer();

        if(dir != NULL) {
            const uint16_t nameLength = 32;
            char fileName[nameLength];

            while(dir->GetNextFile(fileName, nameLength)) {
                String* stationName = new String();
                StoredPagerData pager = serializer.LoadPagerData(
                    &fileManager,
                    stationName,
                    SAVED_STATIONS_PATH,
                    fileName,
                    subghzSettings,
                    [this](const char* name) { return getProtocol(name)->id; },
                    [this](const char* name) { return getDecoder(name)->id; }
                );

                KnownStationData data = getKnownStation(&pager);
                uint32_t stationId = data.toInt();
                knownNames[stationId] = stationName;

                FURI_LOG_I(LOG_TAG, "STID: %lu", stationId);
                FURI_LOG_I(LOG_TAG, "PDATA: %06X", pager.data);
                FURI_LOG_I(LOG_TAG, "ST: %d", data.station);
                FURI_LOG_I(LOG_TAG, "DEC: %d", data.decoder);
                FURI_LOG_I(LOG_TAG, "PROT: %d", data.protocol);
                FURI_LOG_I(LOG_TAG, "FREQ: %d", data.frequency);
                FURI_LOG_I(LOG_TAG, "-------------------------");
            }
        }

        delete dir;
    }

    KnownStationData getKnownStation(StoredPagerData* pager) {
        KnownStationData data;
        data.frequency = pager->frequency;
        data.protocol = pager->protocol;
        data.decoder = pager->decoder;
        data.station = decoders[pager->decoder]->GetStation(pager->data);
        return data;
    }

    PagerProtocol* getProtocol(const char* systemProtocolName) {
        for(size_t i = 0; i < protocols.size(); i++) {
            if(strcmp(systemProtocolName, protocols[i]->GetSystemName()) == 0) {
                return protocols[i];
            }
        }

        return NULL;
    }

    PagerDecoder* getDecoder(const char* shortName) {
        for(size_t i = 0; i < decoders.size(); i++) {
            if(strcmp(shortName, decoders[i]->GetShortName()) == 0) {
                return decoders[i];
            }
        }

        return NULL;
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
        loadKnownStations();
    }

    PagerDataGetter PagerGetter(size_t index) {
        return [this, index]() { return &pagers[index]; };
    }

    String* GetName(StoredPagerData* pager) {
        KnownStationData stData = getKnownStation(pager);
        uint32_t stid = stData.toInt();
        String* name = knownNames[stid];
        FURI_LOG_I(
            LOG_TAG,
            "KN of STID %lu is %s (%d, %d, %d, %d / %d)",
            stid,
            name == NULL ? "NULL" : name->cstr(),
            stData.station,
            stData.decoder,
            stData.protocol,
            stData.frequency,
            stData.unused
        );
        return name;
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
            StoredPagerData storedData = StoredPagerData();
            storedData.data = dataHash;
            storedData.protocol = protocol->id;
            storedData.repeats = 1;
            storedData.te = data->GetTE();
            storedData.frequency = subghzSettings->GetFrequencyIndex(data->GetFrequency());
            storedData.decoder = getDecoder(&storedData)->id;
            storedData.hasName = GetName(&storedData) != NULL;
            storedData.edited = false;

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

        for(const auto& [key, value] : knownNames) {
            delete value;
        }
    }
};
