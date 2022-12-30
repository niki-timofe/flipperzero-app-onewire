#pragma once

#include "ds18b20.h"

typedef struct {
    const OneWireDeviceReader** items;
    const uint16_t size;
} OneWireDeviceReaderRegistry;

extern const OneWireDeviceReaderRegistry onewire_device_reader_registry;

const OneWireDeviceReader* onewire_device_reader_registry_get_by_family(
    const OneWireDeviceReaderRegistry* registry,
    uint8_t family);
