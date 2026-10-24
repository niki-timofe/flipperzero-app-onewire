#include "onewire_worker.h"

#include <furi.h>
#include <furi_hal.h>

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
        // State first: a read interrupted by the host stop must not report an error
        onewire_worker_change_state(instance, OneWireWorkerStateStop);
        onewire_host_stop(instance->onewire_host);
        furi_thread_join(instance->thread);
    }
}

void onewire_worker_search(OneWireWorker* instance) {
    OneWireDeviceData* onewire_device = instance->device_data;

    furi_hal_power_enable_otg();
    onewire_device->first_read = true;

    while(instance->state == OneWireWorkerSearch) {
        if(!onewire_host_search(
               instance->onewire_host, onewire_device->address, OneWireHostSearchModeNormal)) {
            onewire_host_reset_search(instance->onewire_host);
            furi_delay_ms(100);
            continue;
        }

        instance->callback(OneWireWorkerEventDeviceFound, instance->context);
        onewire_host_reset_search(instance->onewire_host);

        const OneWireDeviceReader* reader = onewire_device_reader_registry_get_by_family(
            &onewire_device_reader_registry, onewire_device->address[0]);

        if(reader == NULL) {
            instance->callback(OneWireWorkerEventUnsupportedDeviceError, instance->context);
            continue;
        }

        const bool was_first_read = onewire_device->first_read;
        if(!reader->read(instance->onewire_host, onewire_device)) {
            // Power-on value was discarded: not an error, read again
            const bool discarded = was_first_read && !onewire_device->first_read;
            if(!discarded && instance->state == OneWireWorkerSearch) {
                instance->callback(OneWireWorkerEventReadError, instance->context);
            }
            continue;
        }

        if(instance->state != OneWireWorkerSearch) break;
        instance->callback(OneWireWorkerEventReadSuccess, instance->context);
        break;
    }

    furi_hal_power_disable_otg();
}

// Sleeps in short slices so that stop request is handled promptly
static void onewire_worker_wait(OneWireWorker* instance, uint32_t start, uint32_t duration_ms) {
    while(instance->state == OneWireWorkerLog && furi_get_tick() - start < duration_ms) {
        furi_delay_ms(50);
    }
}

void onewire_worker_log(OneWireWorker* instance) {
    OneWireDeviceData* onewire_device = instance->device_data;
    const OneWireDeviceReader* reader = NULL;

    furi_hal_power_enable_otg();
    onewire_device->first_read = true;

    // Find a supported device first
    while(instance->state == OneWireWorkerLog && reader == NULL) {
        if(!onewire_host_search(
               instance->onewire_host, onewire_device->address, OneWireHostSearchModeNormal)) {
            onewire_host_reset_search(instance->onewire_host);
            furi_delay_ms(100);
            continue;
        }
        onewire_host_reset_search(instance->onewire_host);

        reader = onewire_device_reader_registry_get_by_family(
            &onewire_device_reader_registry, onewire_device->address[0]);
        if(reader == NULL) {
            instance->callback(OneWireWorkerEventUnsupportedDeviceError, instance->context);
            furi_delay_ms(100);
        }
    }

    if(reader != NULL) instance->callback(OneWireWorkerEventDeviceFound, instance->context);

    while(instance->state == OneWireWorkerLog) {
        const uint32_t read_start = furi_get_tick();
        instance->callback(OneWireWorkerEventReadStart, instance->context);
        const bool was_first_read = onewire_device->first_read;
        const bool read_ok = reader->read(instance->onewire_host, onewire_device);
        // A stop request during the read: report nothing
        if(instance->state != OneWireWorkerLog) break;

        if(read_ok) {
            onewire_device->timestamp = furi_hal_rtc_get_timestamp();
            instance->callback(OneWireWorkerEventReadSuccess, instance->context);
        } else if(was_first_read && !onewire_device->first_read) {
            // Power-on value was discarded: not an error, read again right away
            continue;
        } else {
            instance->callback(OneWireWorkerEventReadError, instance->context);
        }

        onewire_worker_wait(instance, read_start, ONEWIRE_WORKER_LOG_INTERVAL_MS);
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

    case OneWireWorkerLog:
        onewire_worker_log(instance);
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

    instance->onewire_host = onewire_host_alloc(&gpio_ibutton);

    onewire_worker_change_state(instance, OneWireWorkerReady);

    return instance;
}

void onewire_worker_free(OneWireWorker* instance) {
    furi_assert(instance);

    furi_thread_free(instance->thread);
    onewire_host_free(instance->onewire_host);

    free(instance);
}
