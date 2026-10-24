#include "../thermometer_i.h"

#define LOG_HEADER "unix_ts,raw,temp_c,addr\n"

static void thermometer_scene_log_worker_callback(OneWireWorkerEvent event, void* context) {
    Thermometer* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void thermometer_scene_log_update(Thermometer* app) {
    FuriString* str = furi_string_alloc();
    Widget* widget = app->widget;

    widget_reset(widget);

    if(app->log_has_value) {
        furi_string_printf(str, "%.4f *C", app->log_last_value);
    } else {
        furi_string_set(str, "Waiting...");
    }
    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontBigNumbers, furi_string_get_cstr(str));

    const uint32_t elapsed = furi_hal_rtc_get_timestamp() - app->log_start_timestamp;
    furi_string_printf(
        str,
        "Rows: %lu\nTime: %02lu:%02lu:%02lu\nErrors: %lu%s",
        app->log_rows,
        elapsed / 3600,
        elapsed / 60 % 60,
        elapsed % 60,
        app->log_errors,
        app->log_write_failed ? "\nSD write failed!" : "");
    widget_add_string_multiline_element(
        widget, 0, 28, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(str));

    furi_string_free(str);
}

// Open for append and close after every row: a crash loses one row at most
static bool thermometer_scene_log_append(Thermometer* app, const char* line) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool success = false;
    if(storage_file_open(
           file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        const size_t size = strlen(line);
        success = storage_file_write(file, line, size) == size;
    }
    storage_file_close(file);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

static void thermometer_scene_log_write_row(Thermometer* app) {
    const OneWireDeviceReader* reader = onewire_device_reader_registry_get_by_family(
        &onewire_device_reader_registry, app->device.address[0]);
    OneWireDeviceDataParsed* parsed = reader->parse(&app->device);

    FuriString* line = furi_string_alloc_printf(
        "%lu,%d,%.4f,", app->device.timestamp, parsed->raw, parsed->value);
    for(size_t i = 0; i < COUNT_OF(app->device.address); i++) {
        furi_string_cat_printf(line, "%02X", app->device.address[i]);
    }
    furi_string_cat(line, "\n");

    if(thermometer_scene_log_append(app, furi_string_get_cstr(line))) {
        app->log_rows++;
        app->log_write_failed = false;
        thermometer_notification_message(app, ThermometerNotificationMessageSdWrite);
    } else {
        app->log_write_failed = true;
        thermometer_notification_message(app, ThermometerNotificationMessageError);
    }
    app->log_last_value = parsed->value;
    app->log_has_value = true;

    furi_string_free(line);
    free(parsed);
}

void thermometer_scene_log_on_enter(void* context) {
    Thermometer* app = context;

    app->log_active = true;
    app->log_rows = 0;
    app->log_errors = 0;
    app->log_has_value = false;
    app->log_write_failed = false;
    app->log_start_timestamp = furi_hal_rtc_get_timestamp();

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(
        app->log_path,
        TM_LOG_DIR "/%04u%02u%02u-%02u%02u%02u.csv",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, TM_LOG_DIR);
    furi_record_close(RECORD_STORAGE);

    app->log_write_failed = !thermometer_scene_log_append(app, LOG_HEADER);

    thermometer_scene_log_update(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewWidget);

    thermometer_notification_message(app, ThermometerNotificationMessageReadStart);

    onewire_worker_start(
        app->onewire_worker,
        OneWireWorkerLog,
        &app->device,
        thermometer_scene_log_worker_callback,
        app);
}

bool thermometer_scene_log_on_event(void* context, SceneManagerEvent event) {
    Thermometer* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case OneWireWorkerEventReadStart:
            thermometer_notification_message(app, ThermometerNotificationMessageReadStart);
            consumed = true;
            break;
        case OneWireWorkerEventReadSuccess:
            thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
            thermometer_scene_log_write_row(app);
            thermometer_scene_log_update(app);
            consumed = true;
            break;
        case OneWireWorkerEventReadError:
            thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
            app->log_errors++;
            thermometer_notification_message(app, ThermometerNotificationMessageError);
            thermometer_scene_log_update(app);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        thermometer_scene_log_update(app);
    }

    return consumed;
}

void thermometer_scene_log_on_exit(void* context) {
    Thermometer* app = context;

    onewire_worker_stop(app->onewire_worker);
    app->log_active = false;

    thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
    widget_reset(app->widget);
}
