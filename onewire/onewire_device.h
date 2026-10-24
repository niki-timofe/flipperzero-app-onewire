#pragma once

#include <furi.h>
#include <lib/one_wire/one_wire_host.h>

typedef struct {
    uint8_t address[8];
    uint8_t data[9];
    // Set by worker when the bus was just powered up (DS18B20 yields 85.0 *C on first read)
    bool first_read;
    // Unix time of the last successful read
    uint32_t timestamp;
} OneWireDeviceData;

typedef enum {
    OneWireDeviceUnitsTemperatureCelsius
} OneWireDeviceUnits;

typedef struct {
    OneWireDeviceUnits units;
    int16_t raw;
    double value;
} OneWireDeviceDataParsed;

typedef bool (*OneWireDeviceRead)(OneWireHost* host, OneWireDeviceData* device);
typedef OneWireDeviceDataParsed* (*OneWireDeviceParse)(OneWireDeviceData* device);
typedef void (*OneWireDevicePrint)(OneWireDeviceData* device, FuriString* string);

typedef struct {
    const uint8_t family;
    const char* family_name;

    OneWireDeviceRead read;
    OneWireDeviceParse parse;
    OneWireDevicePrint print;
} OneWireDeviceReader;

bool onewire_device_is_device_read(OneWireDeviceData* device);
