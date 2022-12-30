#pragma once

#include "onewire_device.h"
#include "devices/registry.h"

typedef enum {
    // Init states
    OneWireWorkerNone,
    OneWireWorkerReady,

    // Main states
    OneWireWorkerSearch,

    // Transition
    OneWireWorkerStateStop,
} OneWireWorkerState;

typedef enum {
    OneWireWorkerEventReserved = 50,
    OneWireWorkerEventDeviceFound,

    // Sensors read
    OneWireWorkerEventReadDS18B20

} OneWireWorkerEvent;

typedef void (*OneWireWorkerCallback)(OneWireWorkerEvent event, void* context);

typedef struct OneWireWorker OneWireWorker;

OneWireWorker* onewire_worker_alloc();
void onewire_worker_free(OneWireWorker* instance);

void onewire_worker_start(
    OneWireWorker* instance,
    OneWireWorkerState state,
    OneWireDeviceData* device_data,
    OneWireWorkerCallback callback,
    void* context);

void onewire_worker_stop(OneWireWorker* instance);
