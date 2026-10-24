#pragma once

#include "thermometer.h"
#include "onewire/onewire_worker.h"

#include <furi.h>
#include <dolphin/dolphin.h>

#include <thermometer_icons.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <furi_hal.h>

#include <lib/one_wire/one_wire_host.h>

#include "scenes/thermometer_scene.h"

#define TM_APP_VERSION "0.2.1"
#define TM_LOG_DIR     "/ext/apps_data/thermometer"

// Copy of the state published by the app thread, so that the CLI thread never reads live fields
typedef struct {
    bool log_active;
    char log_path[64];
    uint32_t log_rows;
    uint32_t log_errors;
    uint32_t log_start_timestamp;
    bool log_has_value;
    bool device_valid;
    uint32_t read_seq; // Incremented with every successful reading
    OneWireDeviceData device;
} ThermometerStatus;

struct Thermometer {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    View* log_view;

    OneWireWorker* onewire_worker;
    OneWireDeviceData device;

    // Logging state
    FuriString* log_path;
    uint32_t log_start_timestamp;
    uint32_t log_rows;
    uint32_t log_errors;
    bool log_has_value;
    double log_last_value;
    bool log_write_failed;
    bool log_active;

    // Shared with the CLI
    FuriMutex* status_mutex;
    ThermometerStatus status;
    volatile uint32_t cli_running;
    volatile bool exiting;
};

typedef enum {
    ThermometerViewSubmenu,
    ThermometerViewWidget,
    ThermometerViewPopup,
    ThermometerViewLog,
} ThermometerView;

// Custom events sent by the CLI commands, handled in thermometer_custom_event_callback
typedef enum {
    ThermometerCustomEventCliRead = 100,
    ThermometerCustomEventCliLogStart,
    ThermometerCustomEventCliLogStop,
    ThermometerCustomEventLogStop,
} ThermometerCustomEvent;

View* thermometer_scene_log_view_alloc(Thermometer* app);

void thermometer_scene_log_view_free(View* view);

// Called from the app thread: publishes the log state, and the device if it was just read
void thermometer_status_publish(Thermometer* app, bool new_reading);

void thermometer_status_get(Thermometer* app, ThermometerStatus* status);
