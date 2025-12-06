// protocols/protocol_items.c
#include "protocol_items.h"

const SubGhzProtocol* kia_protocol_registry_items[] = {
    &kia_protocol_v0,
    &kia_protocol_v1,
    &kia_protocol_v2,
    &kia_protocol_v4,
    &kia_protocol_v5,
};

const SubGhzProtocolRegistry kia_protocol_registry = {
    .items = kia_protocol_registry_items,
    .size = COUNT_OF(kia_protocol_registry_items),
};