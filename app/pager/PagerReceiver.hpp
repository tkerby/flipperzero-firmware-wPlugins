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
    map<uint32_t, String*> knownStations;
    bool knownStationsLoaded = false;

    void loadKnownStations(bool withNames) {
        FileManager fileManager = FileManager();
        Directory* dir = fileManager.OpenDirectory(SAVED_STATIONS_PATH);
        PagerSerializer serializer = PagerSerializer();

        if(dir != NULL) {
            char fileName[MAX_STATION_FILENAME_LENGTH];

            while(dir->GetNextFile(fileName, MAX_STATION_FILENAME_LENGTH)) {
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

                addKnown(withNames, stationName, &pager);
            }
        }

        knownStationsLoaded = true;
        delete dir;
    }

    void addKnown(bool withName, String* stationName, StoredPagerData* pager) {
        KnownStationData data = getKnownStation(pager);
        uint32_t stationId = data.toInt();
        if(withName && knownStations.contains(stationId)) {
            delete knownStations[stationId];
        }
        knownStations[stationId] = withName ? stationName : NULL;

        if(!withName) {
            delete stationName;
        }
    }

    void unloadKnownStations() {
        for(const auto& [key, value] : knownStations) {
            if(value != NULL) {
                delete value;
            }
        }

        knownStationsLoaded = false;
        knownStations.clear();
    }

    KnownStationData getKnownStation(StoredPagerData* pager) {
        KnownStationData data = KnownStationData();
        data.frequency = pager->frequency;
        data.protocol = pager->protocol;
        data.decoder = pager->decoder;
        data.station = decoders[pager->decoder]->GetStation(pager->data);
        return data;
    }

    PagerDecoder* getDecoder(StoredPagerData* pagerData) {
        for(size_t i = 0; i < decoders.size(); i++) {
            pagerData->decoder = i;
            if(IsKnown(pagerData)) {
                return decoders[i];
            }
        }

        for(size_t i = 0; i < decoders.size(); i++) {
            if(decoders[i]->GetPager(pagerData->data) <= config->MaxPagerForBatchOrDetection) {
                return decoders[i];
            }
        }

        return decoders[0];
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
    }

    void ReloadKnownStations() {
        unloadKnownStations();

        if(config->SavedStrategy == SHOW_NAME) {
            loadKnownStations(true);
        } else {
            loadKnownStations(false);
        }
    }

    void LoadStationsFromDirectory(const char* stationDirectory, function<void(ReceivedPagerData*)> pagerHandler) {
        FileManager fileManager = FileManager();
        Directory* dir = fileManager.OpenDirectory(stationDirectory);
        PagerSerializer serializer = PagerSerializer();
        bool withNames = config->SavedStrategy == SHOW_NAME;

        if(dir != NULL) {
            char fileName[MAX_STATION_FILENAME_LENGTH];

            while(dir->GetNextFile(fileName, MAX_STATION_FILENAME_LENGTH)) {
                FURI_LOG_I(LOG_TAG, "Reading file %s", fileName);

                String* stationName = new String();
                StoredPagerData pager = serializer.LoadPagerData(
                    &fileManager,
                    stationName,
                    stationDirectory,
                    fileName,
                    subghzSettings,
                    [this](const char* name) { return getProtocol(name)->id; },
                    [this](const char* name) { return getDecoder(name)->id; }
                );

                if(!knownStationsLoaded) {
                    FURI_LOG_I(LOG_TAG, "Adding known station %s", stationName->cstr());
                    addKnown(withNames, stationName, &pager);
                } else {
                    delete stationName;
                }

                int index = pagers.size();
                pagers.push_back(pager);
                pagerHandler(new ReceivedPagerData(PagerGetter(index), index, true));
            }
        }

        delete dir;
    }

    PagerDataGetter PagerGetter(size_t index) {
        return [this, index]() { return &pagers[index]; };
    }

    String* GetName(StoredPagerData* pager) {
        uint32_t stationId = getKnownStation(pager).toInt();
        if(knownStations.contains(stationId)) {
            return knownStations[stationId];
        }
        return NULL;
    }

    bool IsKnown(StoredPagerData* pager) {
        for(const auto& [key, value] : knownStations) {
            FURI_LOG_I(LOG_TAG, "IN: %lu", key);
        }
        bool result = knownStations.contains(getKnownStation(pager).toInt());
        FURI_LOG_I(LOG_TAG, "KS contains %lu: %d", getKnownStation(pager).toInt(), result);
        return result;
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
            storedData.edited = false;

            if(config->SavedStrategy == HIDE && IsKnown(&storedData)) {
                return NULL;
            }

            if(config->AutosaveFoundSignals) {
                DateTime datetime;
                furi_hal_rtc_get_datetime(&datetime);
                String todaysDir =
                    String("%s/%d-%02d-%02d", AUTOSAVED_STATIONS_PATH, datetime.year, datetime.month, datetime.day);

                FileManager fileManager = FileManager();
                fileManager.CreateDirIfNotExists(STATIONS_PATH);
                fileManager.CreateDirIfNotExists(AUTOSAVED_STATIONS_PATH);
                fileManager.CreateDirIfNotExists(todaysDir.cstr());

                PagerSerializer().SavePagerData(
                    &fileManager, todaysDir.cstr(), "", &storedData, decoders[storedData.decoder], protocol, subghzSettings
                );
            }

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

        unloadKnownStations();
    }
};
