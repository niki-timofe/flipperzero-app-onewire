#include "onewire_worker.h"

#include <furi.h>
#include <furi_hal.h>

#include <lib/one_wire/one_wire_host.h>

#define TAG "OneWireWorker"

struct OneWireWorker {
    FuriThread* thread;
    OneWireHost* onewire_host;

    OneWireDeviceData* device_data;

    OneWireWorkerState state;

    OneWireWorkerCallback callback;
    void* context;
};

void onewire_worker_change_state(OneWireWorker* instance, OneWireWorkerState state) {
    instance->state = state;
}

void onewire_worker_start(
    OneWireWorker* instance,
    OneWireWorkerState state,
    OneWireDeviceData* device_data,
    OneWireWorkerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(device_data);

    instance->callback = callback;
    instance->context = context;
    instance->device_data = device_data;
    onewire_worker_change_state(instance, state);
    furi_thread_start(instance->thread);
}

void onewire_worker_stop(OneWireWorker* instance) {
    furi_assert(instance);
    if(furi_thread_get_state(instance->thread) != FuriThreadStateStopped) {
        onewire_host_stop(instance->onewire_host);
        onewire_worker_change_state(instance, OneWireWorkerStateStop);
        furi_thread_join(instance->thread);
    }
}

void onewire_worker_search(OneWireWorker* instance) {
    OneWireDeviceData* onewire_device = instance->device_data;

    furi_hal_power_enable_otg();

    while(instance->state == OneWireWorkerSearch) {
        if(onewire_host_search(instance->onewire_host, onewire_device->address, NORMAL_SEARCH)) {
            instance->callback(OneWireWorkerEventDeviceFound, instance->context);
            onewire_host_reset_search(instance->onewire_host);

            const OneWireDeviceReader* reader = onewire_device_reader_registry_get_by_family(
                &onewire_device_reader_registry, onewire_device->address[0]);

            if(reader != NULL) {
                instance->callback(OneWireWorkerEventReadDS18B20, instance->context);

                if(reader->read(instance->onewire_host, onewire_device)) {
                    break;
                }
            }
        }

        furi_delay_ms(100);
    }

    furi_hal_power_disable_otg();
}

static int32_t onewire_worker_task(void* context) {
    OneWireWorker* instance = context;

    onewire_host_start(instance->onewire_host);

    switch(instance->state) {
    case OneWireWorkerSearch:
        onewire_worker_search(instance);
        break;

    default:
        onewire_worker_change_state(instance, OneWireWorkerReady);
        break;
    }

    return 0;
}

OneWireWorker* onewire_worker_alloc() {
    OneWireWorker* instance = malloc(sizeof(OneWireWorker));

    instance->thread = furi_thread_alloc_ex("OneWireWorker", 2048, onewire_worker_task, instance);

    instance->onewire_host = onewire_host_alloc();

    onewire_worker_change_state(instance, OneWireWorkerReady);

    return instance;
}

void onewire_worker_free(OneWireWorker* instance) {
    furi_assert(instance);

    furi_thread_free(instance->thread);
    onewire_host_free(instance->onewire_host);

    free(instance);
}
