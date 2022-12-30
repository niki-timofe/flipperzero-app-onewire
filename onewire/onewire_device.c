#include "onewire_device.h"

bool onewire_device_is_device_read(OneWireDeviceData* device) {
    return device->address[0] != 0;
}
