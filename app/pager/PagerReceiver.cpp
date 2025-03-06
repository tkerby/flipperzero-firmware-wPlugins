#ifndef _PAGER_RECEIVER_CLASS_
#define _PAGER_RECEIVER_CLASS_

#include <vector>

#include "lib/hardware/subghz/SubGhzReceivedData.cpp"
#include "PagerData.cpp"

#include "protocol/PrincetonProtocol.cpp"
#include "protocol/Smc5326Protocol.cpp"

#undef LOG_TAG
#define LOG_TAG "PGR_RCV"

#define MAX_REPEATS 99

using namespace std;

class PagerReceiver {
private:
    vector<PagerDataStored> pagers;
    vector<PagerProtocol*> protocols = {new PrincetonProtocol(), new Smc5326Protocol()};

    PagerProtocol* getProtocol(const char* systemProtocolName) {
        for(size_t i = 0; i < protocols.size(); i++) {
            if(strcmp(systemProtocolName, protocols[i]->GetSystemName()) == 0) {
                return protocols[i];
            }
        }
        return NULL;
    }

public:
    PagerReceiver() {
        for(size_t i = 0; i < protocols.size(); i++) {
            protocols[i]->id = i;
        }
    }

    PagerData* Receive(SubGhzReceivedData data) {
        PagerProtocol* protocol = getProtocol(data.GetProtocolName());
        if(protocol == NULL) {
            FURI_LOG_I(LOG_TAG, "Skipping received data with unsupported protocol: %s", data.GetProtocolName());
            return NULL;
        }

        PagerDataStored dataToStore;
        dataToStore.data = data.GetHash();
        dataToStore.protocol = protocol->id;
        dataToStore.repeats = 1;

        int indexFoundOn = -1;
        for(size_t i = 0; i < pagers.size(); i++) {
            if(pagers[i].data == dataToStore.data && pagers[i].protocol == dataToStore.protocol) {
                if(pagers[i].repeats < MAX_REPEATS) {
                    pagers[i].repeats++;
                }
                dataToStore = pagers[i];
                indexFoundOn = i;
                break;
            }
        }

        if(indexFoundOn < 0) {
            pagers.push_back(dataToStore);
            return new PagerData(protocol, dataToStore);
        }

        return new PagerData(protocol, dataToStore, indexFoundOn);
    }
};

#endif //_PAGER_RECEIVER_CLASS_
