#include "ds18b20.h"

const double resolution[] = {0.5, 0.25, 0.125, 0.0625};

void onewire_device_reader_ds18b20_request_conversion(OneWireHost* host) {
    FURI_CRITICAL_ENTER()
    onewire_host_reset(host);
    onewire_host_write(host, 0xCCU);
    onewire_host_write(host, 0x44);
    FURI_CRITICAL_EXIT()
}

void onewire_device_reader_ds18b20_read_scratchpad(
    OneWireHost* host,
    OneWireDeviceData* onewire_device) {
    FURI_CRITICAL_ENTER()
    onewire_host_reset(host);
    onewire_host_write(host, 0xCCU);
    onewire_host_write(host, 0xBE);
    onewire_host_read_bytes(host, onewire_device->data, 8);
    onewire_host_reset(host);
    FURI_CRITICAL_EXIT()
}

bool onewire_device_reader_ds18b20_read(OneWireHost* host, OneWireDeviceData* onewire_device) {
    onewire_device_reader_ds18b20_request_conversion(host);
    furi_delay_ms(800);
    onewire_device_reader_ds18b20_read_scratchpad(host, onewire_device);
    return true;
}

OneWireDeviceDataParsed* onewire_device_reader_ds18b20_parse(OneWireDeviceData* onewire_device) {
    OneWireDeviceDataParsed* data = malloc(sizeof(OneWireDeviceDataParsed));

    data->units = OneWireDeviceUnitsTemperatureCelsius;

    double divider = resolution[onewire_device->data[4] >> 5];

    data->value =
        (double)(int16_t)(onewire_device->data[0] | onewire_device->data[1] << 8) * divider;

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

    furi_string_cat_printf(string, "Resolution: %.4f\n", resolution[onewire_device->data[4] >> 5]);
    furi_string_cat_printf(string, "Temperature: %.4f*C", data->value);
}

const OneWireDeviceReader onewire_device_reader_ds18b20 = {
    .family = 0x28,
    .family_name = "DS18B20",
    .read = onewire_device_reader_ds18b20_read,
    .parse = onewire_device_reader_ds18b20_parse,
    .print = onewire_device_reader_ds18b20_print};
