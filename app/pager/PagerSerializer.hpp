#pragma once

#include "lib/String.hpp"
#include "lib/file/FileManager.hpp"
#include "lib/file/FlipperFile.hpp"

#include "PagerDataStored.hpp"
#include "lib/hardware/subghz/SubGhzSettings.hpp"
#include "protocol/PagerProtocol.hpp"
#include "decoder/PagerDecoder.hpp"

#define KEY_PAGER_STATION_NAME "StationName"
#define KEY_PAGER_FREQUENCY    "Frequency"
#define KEY_PAGER_PROTOCOL     "Protocol"
#define KEY_PAGER_DECODER      "Decoder"
#define KEY_PAGER_DATA         "Data"
#define KEY_PAGER_TE           "TE"

class PagerSerializer {
private:
public:
    void SavePagerData(
        FileManager* fileManager,
        const char* dir,
        const char* stationName,
        PagerDataStored* pager,
        PagerDecoder* decoder,
        PagerProtocol* protocol,
        SubGhzSettings* settings
    ) {
        String fileName = String("%s-%d-%06X.fff", decoder->GetShortName(), decoder->GetStation(pager->data), pager->data);
        FlipperFile* stationFile = fileManager->OpenWrite(dir, fileName.cstr());

        stationFile->WriteString(KEY_PAGER_STATION_NAME, stationName);
        stationFile->WriteUInt32(KEY_PAGER_FREQUENCY, settings->GetFrequency(pager->frequency));
        stationFile->WriteString(KEY_PAGER_PROTOCOL, protocol->GetDisplayName());
        stationFile->WriteString(KEY_PAGER_DECODER, decoder->GetShortName());
        stationFile->WriteUInt32(KEY_PAGER_TE, pager->te);
        stationFile->WriteHex(KEY_PAGER_DATA, pager->data);

        delete stationFile;
    }
};
