#include "ds18b20.h"

const float resolution[] = {0.5, 0.25, 0.125, 0.0625};

bool onewire_device_reader_ds18b20_read(OneWireHost* host, OneWireDeviceData* onewire_device) {
    FURI_CRITICAL_ENTER();
    onewire_host_reset(host);
    onewire_host_skip(host);
    onewire_host_write(host, 0x44);
    furi_delay_ms(800);
    onewire_host_reset(host);
    onewire_host_skip(host);
    onewire_host_write(host, 0xBE);
    onewire_host_read_bytes(host, onewire_device->data, 8);
    onewire_host_reset(host);
    FURI_CRITICAL_EXIT();

    return true;
}

OneWireDeviceDataParsed* onewire_device_reader_ds18b20_parse(OneWireDeviceData* onewire_device) {
    OneWireDeviceDataParsed* data = malloc(sizeof(OneWireDeviceDataParsed));

    data->units = OneWireDeviceUnitsTemperatureCelsius;

    float divider = resolution[onewire_device->data[4] >> 5];

    data->value =
        (float)(int16_t)(onewire_device->data[0] | onewire_device->data[1] << 8) * divider;

    return data;
}

const OneWireDeviceReader onewire_device_reader_ds18b20 = {
    .family = 0x28,
    .family_name = "DS18B20",
    .read = onewire_device_reader_ds18b20_read,
    .parse = onewire_device_reader_ds18b20_parse,
};
