#include "ds18b20.h"

#include <lib/one_wire/maxim_crc.h>

#define DS18B20_SCRATCHPAD_SIZE 9
#define DS18B20_POWER_ON_RAW    0x0550 // 85.0 *C, scratchpad value after power-on reset

// Resolution in bits by config register bits 5-6
static const uint8_t resolution_bits[] = {9, 10, 11, 12};

static uint8_t ds18b20_resolution_index(const OneWireDeviceData* onewire_device) {
    return (onewire_device->data[4] >> 5) & 0x03;
}

static int16_t ds18b20_raw_from(const uint8_t* scratchpad) {
    uint16_t raw = scratchpad[0] | scratchpad[1] << 8;
    // Lower bits are undefined at resolutions below 12 bit
    raw &= ~((1U << (3 - ((scratchpad[4] >> 5) & 0x03))) - 1);
    return (int16_t)raw;
}

static int16_t ds18b20_raw(const OneWireDeviceData* onewire_device) {
    return ds18b20_raw_from(onewire_device->data);
}

void onewire_device_reader_ds18b20_request_conversion(OneWireHost* host) {
    FURI_CRITICAL_ENTER();
    onewire_host_reset(host);
    onewire_host_write(host, 0xCCU);
    onewire_host_write(host, 0x44);
    FURI_CRITICAL_EXIT();
}

static bool onewire_device_reader_ds18b20_read_scratchpad(OneWireHost* host, uint8_t* scratchpad) {
    FURI_CRITICAL_ENTER();
    bool present = onewire_host_reset(host);
    if(present) {
        onewire_host_write(host, 0xCCU);
        onewire_host_write(host, 0xBE);
        onewire_host_read_bytes(host, scratchpad, DS18B20_SCRATCHPAD_SIZE);
        onewire_host_reset(host);
    }
    FURI_CRITICAL_EXIT();
    return present;
}

bool onewire_device_reader_ds18b20_read(OneWireHost* host, OneWireDeviceData* onewire_device) {
    onewire_device_reader_ds18b20_request_conversion(host);
    furi_delay_ms(800);

    // The device data is only touched by a reading that passed all the checks
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
    if(!onewire_device_reader_ds18b20_read_scratchpad(host, scratchpad)) return false;

    if(maxim_crc8(scratchpad, DS18B20_SCRATCHPAD_SIZE, MAXIM_CRC8_INIT) != 0) {
        return false;
    }

    if(onewire_device->first_read) {
        onewire_device->first_read = false;
        if(ds18b20_raw_from(scratchpad) == DS18B20_POWER_ON_RAW) return false;
    }

    memcpy(onewire_device->data, scratchpad, DS18B20_SCRATCHPAD_SIZE);
    return true;
}

OneWireDeviceDataParsed* onewire_device_reader_ds18b20_parse(OneWireDeviceData* onewire_device) {
    OneWireDeviceDataParsed* data = malloc(sizeof(OneWireDeviceDataParsed));

    data->units = OneWireDeviceUnitsTemperatureCelsius;

    data->raw = ds18b20_raw(onewire_device);
    data->value = (double)((float)data->raw / 16.0f);

    return data;
}

void onewire_device_reader_ds18b20_print(OneWireDeviceData* onewire_device, FuriString* string) {
    furi_string_cat(string, "Addr: ");
    for(uint32_t i = 0; i < COUNT_OF(onewire_device->address); ++i) {
        if(i > 0) furi_string_cat(string, " ");
        furi_string_cat_printf(string, "%02X", onewire_device->address[i]);
    }
    furi_string_cat(string, "\n");

    OneWireDeviceDataParsed* data = onewire_device_reader_ds18b20_parse(onewire_device);

    furi_string_cat_printf(
        string, "Resolution: %u bit\n", resolution_bits[ds18b20_resolution_index(onewire_device)]);
    furi_string_cat_printf(string, "Temperature: %.4f*C", data->value);
    free(data);
}

const OneWireDeviceReader onewire_device_reader_ds18b20 = {
    .family = 0x28,
    .family_name = "DS18B20",
    .read = onewire_device_reader_ds18b20_read,
    .parse = onewire_device_reader_ds18b20_parse,
    .print = onewire_device_reader_ds18b20_print};
