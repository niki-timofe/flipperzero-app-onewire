#pragma once

#include <furi.h>
#include <lib/one_wire/one_wire_host.h>

typedef struct {
    uint8_t address[8];
    uint8_t data[8];
} OneWireDeviceData;

typedef enum { OneWireDeviceUnitsTemperatureCelsius } OneWireDeviceUnits;

typedef struct {
    OneWireDeviceUnits units;
    float value;
} OneWireDeviceDataParsed;

typedef bool (*OneWireDeviceRead)(OneWireHost* host, OneWireDeviceData* device);
typedef OneWireDeviceDataParsed* (*OneWireDeviceParse)(OneWireDeviceData* device);

typedef struct {
    const uint8_t family;
    const char* family_name;

    OneWireDeviceRead read;
    OneWireDeviceParse parse;
} OneWireDeviceReader;

bool onewire_device_is_device_read(OneWireDeviceData* device);
