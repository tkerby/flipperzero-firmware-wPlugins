#pragma once

#include "PagerSerializer.hpp"
#include <cstring>
#include <forward_list>

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

#define MAX_REPEATS                  99
#define PAGERS_ARRAY_SIZE_MULTIPLIER 8

using namespace std;

class PagerReceiver {
public:
    static const uint8_t protocolsCount = 2;
    PagerProtocol* protocols[protocolsCount]{
        new PrincetonProtocol(),
        new Smc5326Protocol(),
    };

    static const uint8_t decodersCount = 5;
    PagerDecoder* decoders[decodersCount]{
        new Td157Decoder(),
        new Td165Decoder(),
        new Td174Decoder(),
        new L8RDecoder(),
        new L8SDecoder(),
    };

private:
    AppConfig* config;
    uint16_t pagersArraySize = PAGERS_ARRAY_SIZE_MULTIPLIER;
    uint16_t nextPagerIndex = 0;
    StoredPagerData* pagers = new StoredPagerData[pagersArraySize];
    size_t knownStationsSize = 0;
    KnownStationData* knownStations;
    uint32_t lastFrequency = 0;
    uint8_t lastFrequencyIndex = 0;
    bool knownStationsLoaded = false;

    void loadKnownStations(bool withNames) {
        FileManager fileManager = FileManager();
        Directory* dir = fileManager.OpenDirectory(SAVED_STATIONS_PATH);
        PagerSerializer serializer = PagerSerializer();
        SubGhzSettings subghzSettings = SubGhzSettings();
        forward_list<KnownStationData> stations;

        if(dir != NULL) {
            char fileName[MAX_STATION_FILENAME_LENGTH];

            while(dir->GetNextFile(fileName, MAX_STATION_FILENAME_LENGTH)) {
                String* stationName = new String();
                StoredPagerData pager = serializer.LoadPagerData(
                    &fileManager,
                    stationName,
                    SAVED_STATIONS_PATH,
                    fileName,
                    &subghzSettings,
                    [this](const char* name) { return getProtocol(name)->id; },
                    [this](const char* name) { return getDecoder(name)->id; }
                );
                addKnown(stations, withNames, pager, stationName);
                knownStationsSize++;
            }
        }

        copyKnown(&stations);
        delete dir;
    }

    void addKnown(forward_list<KnownStationData>& stations, bool withNames, StoredPagerData pager, String* stationName) {
        KnownStationData data = getKnownStation(&pager);
        if(withNames) {
            data.name = stationName;
        } else {
            data.name = NULL;
            delete stationName;
        }
        stations.push_front(data);
    }

    void copyKnown(forward_list<KnownStationData>* stations) {
        knownStations = new KnownStationData[knownStationsSize];
        for(size_t i = 0; i < knownStationsSize; i++) {
            knownStations[i] = stations->front();
            stations->pop_front();
        }

        knownStationsLoaded = true;
    }

    void unloadKnownStations() {
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].name != NULL) {
                delete knownStations[i].name;
            }
        }

        delete[] knownStations;

        knownStationsLoaded = false;
        knownStationsSize = 0;
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
        for(size_t i = 0; i < decodersCount; i++) {
            pagerData->decoder = i;
            if(IsKnown(pagerData)) {
                return decoders[i];
            }
        }

        for(size_t i = 0; i < decodersCount; i++) {
            if(decoders[i]->GetPager(pagerData->data) <= config->MaxPagerForBatchOrDetection) {
                return decoders[i];
            }
        }

        return decoders[0];
    }

    PagerProtocol* getProtocol(const char* systemProtocolName) {
        for(size_t i = 0; i < protocolsCount; i++) {
            if(strcmp(systemProtocolName, protocols[i]->GetSystemName()) == 0) {
                return protocols[i];
            }
        }

        return NULL;
    }

    PagerDecoder* getDecoder(const char* shortName) {
        for(size_t i = 0; i < decodersCount; i++) {
            if(strcmp(shortName, decoders[i]->GetShortName()) == 0) {
                return decoders[i];
            }
        }

        return NULL;
    }

    void addPager(StoredPagerData data) {
        if(nextPagerIndex == pagersArraySize) {
            pagersArraySize += PAGERS_ARRAY_SIZE_MULTIPLIER;
            StoredPagerData* newPagers = new StoredPagerData[pagersArraySize];
            for(int i = 0; i < nextPagerIndex; i++) {
                newPagers[i] = pagers[i];
            }
            delete[] pagers;
            pagers = newPagers;
        }
        pagers[nextPagerIndex++] = data;
    }

public:
    PagerReceiver(AppConfig* config) {
        this->config = config;

        for(size_t i = 0; i < protocolsCount; i++) {
            protocols[i]->id = i;
        }

        for(size_t i = 0; i < decodersCount; i++) {
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
        SubGhzSettings subghzSettings = SubGhzSettings();
        bool withNames = config->SavedStrategy == SHOW_NAME;
        forward_list<KnownStationData> stations;

        if(dir != NULL) {
            char fileName[MAX_STATION_FILENAME_LENGTH];

            while(dir->GetNextFile(fileName, MAX_STATION_FILENAME_LENGTH)) {
                String* stationName = new String();
                StoredPagerData pager = serializer.LoadPagerData(
                    &fileManager,
                    stationName,
                    stationDirectory,
                    fileName,
                    &subghzSettings,
                    [this](const char* name) { return getProtocol(name)->id; },
                    [this](const char* name) { return getDecoder(name)->id; }
                );

                if(!knownStationsLoaded) {
                    addKnown(stations, withNames, pager, stationName);
                    knownStationsSize++;
                } else {
                    delete stationName;
                }

                int index = nextPagerIndex;
                addPager(pager);
                pagerHandler(new ReceivedPagerData(PagerGetter(index), index, true));
            }
        }

        if(!knownStationsLoaded) {
            copyKnown(&stations);
        }
        delete dir;
    }

    PagerDataGetter PagerGetter(size_t index) {
        return [this, index]() { return &pagers[index]; };
    }

    String* GetName(StoredPagerData* pager) {
        uint32_t stationId = getKnownStation(pager).toInt();
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].toInt() == stationId) {
                return knownStations[i].name;
            }
        }
        return NULL;
    }

    bool IsKnown(StoredPagerData* pager) {
        uint32_t stationId = getKnownStation(pager).toInt();
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].toInt() == stationId) {
                return true;
            }
        }
        return false;
    }

    ReceivedPagerData* Receive(SubGhzReceivedData* data) {
        PagerProtocol* protocol = getProtocol(data->GetProtocolName());
        if(protocol == NULL) {
            return NULL;
        }

        int index = -1;
        uint32_t dataHash = data->GetHash();

        for(size_t i = 0; i < nextPagerIndex; i++) {
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
            if(data->GetFrequency() != lastFrequency) {
                lastFrequencyIndex = SubGhzSettings().GetFrequencyIndex(data->GetFrequency());
                lastFrequency = data->GetFrequency();
            }

            StoredPagerData storedData = StoredPagerData();
            storedData.data = dataHash;
            storedData.protocol = protocol->id;
            storedData.repeats = 1;
            storedData.te = data->GetTE();
            storedData.frequency = lastFrequencyIndex;
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
                    &fileManager, todaysDir.cstr(), "", &storedData, decoders[storedData.decoder], protocol, lastFrequency
                );
            }

            index = nextPagerIndex;
            addPager(storedData);
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

        delete[] pagers;
        unloadKnownStations();
    }
};
