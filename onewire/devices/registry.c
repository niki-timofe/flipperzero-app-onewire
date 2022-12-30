#include "registry.h"

const OneWireDeviceReader* onewire_device_readers[] = {
    &onewire_device_reader_ds18b20,
};

const OneWireDeviceReaderRegistry onewire_device_reader_registry = {
    .items = onewire_device_readers,
    .size = COUNT_OF(onewire_device_readers)};

const OneWireDeviceReader* onewire_device_reader_registry_get_by_family(
    const OneWireDeviceReaderRegistry* registry,
    uint8_t family) {
    for(size_t i = 0; i < registry->size; i++) {
        if(registry->items[i]->family == family) {
            return registry->items[i];
        }
    }

    return NULL;
}
