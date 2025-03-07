#ifndef _PAGER_RECEIVER_CLASS_
#define _PAGER_RECEIVER_CLASS_

#include <vector>

#include "lib/hardware/subghz/data/SubGhzReceivedData.cpp"
#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.cpp"
#include "PagerData.cpp"

#include "protocol/PrincetonProtocol.cpp"
#include "protocol/Smc5326Protocol.cpp"

#include "decoder/Td157Decoder.cpp"
#include "decoder/Td165Decoder.cpp"
#include "decoder/Td174Decoder.cpp"

#undef LOG_TAG
#define LOG_TAG "PGR_RCV"

#define MAX_REPEATS 99

using namespace std;

class PagerReceiver {
public:
    vector<PagerProtocol*> protocols = {new PrincetonProtocol(), new Smc5326Protocol()};
    vector<PagerDecoder*> decoders = {new Td157Decoder(), new Td165Decoder(), new Td174Decoder()};

private:
    int pagerNumThreshold = 50;
    vector<PagerDataStored*> pagers;

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
            if(decoders[i]->GetPager(pagerData->data) <= pagerNumThreshold) {
                return decoders[i];
            }
        }
        return decoders[0];
    }

public:
    PagerReceiver() {
        for(size_t i = 0; i < protocols.size(); i++) {
            protocols[i]->id = i;
        }
        for(size_t i = 0; i < decoders.size(); i++) {
            decoders[i]->id = i;
        }
    }

    PagerDataStored* GetPagerData(size_t index) {
        return pagers[index];
    }

    PagerData* Receive(SubGhzReceivedData* data) {
        PagerProtocol* protocol = getProtocol(data->GetProtocolName());
        if(protocol == NULL) {
            FURI_LOG_I(LOG_TAG, "Skipping received data with unsupported protocol: %s", data->GetProtocolName());
            return NULL;
        }

        PagerDataStored* dataToStore = new PagerDataStored();
        dataToStore->data = data->GetHash();
        dataToStore->protocol = protocol->id;
        dataToStore->repeats = 1;

        int indexFoundOn = -1;
        for(size_t i = 0; i < pagers.size(); i++) {
            if(pagers[i]->data == dataToStore->data && pagers[i]->protocol == dataToStore->protocol) {
                if(pagers[i]->repeats < MAX_REPEATS) {
                    pagers[i]->repeats++;
                } else {
                    return NULL; // no need to modify element any more
                }
                delete dataToStore;
                dataToStore = pagers[i];
                indexFoundOn = i;
                break;
            }
        }

        if(indexFoundOn < 0) {
            PagerDecoder* decoder = getDecoder(dataToStore);
            dataToStore->decoder = decoder->id;

            pagers.push_back(dataToStore);
            return new PagerData(dataToStore, protocol, decoder);
        }

        PagerDecoder* decoder = decoders[dataToStore->decoder];
        return new PagerData(dataToStore, protocol, decoder, indexFoundOn);
    }
};

#endif //_PAGER_RECEIVER_CLASS_
