#include "../thermometer_i.h"

#define LOG_HEADER "unix_ts,raw,temp_c,addr\n"

typedef struct {
    char file_name[24];
    bool has_value;
    double value;
    uint32_t rows;
    uint32_t errors;
    uint32_t elapsed;
    bool write_failed;
    bool confirm_stop;
} ThermometerLogViewModel;

static void thermometer_scene_log_worker_callback(OneWireWorkerEvent event, void* context) {
    Thermometer* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void thermometer_scene_log_draw_confirm(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Stop logging?");
    elements_button_left(canvas, "Cancel");
    elements_button_right(canvas, "Stop");
}

static void thermometer_scene_log_draw_callback(Canvas* canvas, void* _model) {
    ThermometerLogViewModel* model = _model;
    FuriString* str = furi_string_alloc();

    canvas_clear(canvas);

    // Recording indicator blinks once a second, next to the file name
    if(model->elapsed % 2 == 0) {
        canvas_draw_box(canvas, 0, 3, 5, 5);
    } else {
        canvas_draw_frame(canvas, 0, 3, 5, 5);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 9, 0, AlignLeft, AlignTop, model->file_name);

    if(model->confirm_stop) {
        thermometer_scene_log_draw_confirm(canvas);
        furi_string_free(str);
        return;
    }

    if(model->has_value) {
        furi_string_printf(str, "%.4f", model->value);

        // Number and unit are centred together; the font has no degree sign,
        // so it is drawn as a small circle before the "C"
        canvas_set_font(canvas, FontBigNumbers);
        const uint8_t number_width = canvas_string_width(canvas, furi_string_get_cstr(str));
        canvas_set_font(canvas, FontPrimary);
        const uint8_t unit_width = 7 + canvas_string_width(canvas, "C");
        const uint8_t x = (canvas_width(canvas) - number_width - unit_width) / 2;

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x, 13, AlignLeft, AlignTop, furi_string_get_cstr(str));
        canvas_draw_circle(canvas, x + number_width + 4, 22, 2);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + number_width + 8, 21, AlignLeft, AlignTop, "C");
    } else {
        // FontBigNumbers has no letters
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Waiting...");
    }

    canvas_set_font(canvas, FontSecondary);
    furi_string_printf(str, "Rows %lu", model->rows);
    canvas_draw_str_aligned(canvas, 0, 35, AlignLeft, AlignTop, furi_string_get_cstr(str));

    furi_string_printf(str, "Err %lu", model->errors);
    canvas_draw_str_aligned(canvas, 127, 35, AlignRight, AlignTop, furi_string_get_cstr(str));

    if(model->write_failed) {
        furi_string_set(str, "SD write failed!");
    } else {
        furi_string_printf(
            str,
            "%02lu:%02lu:%02lu",
            model->elapsed / 3600,
            model->elapsed / 60 % 60,
            model->elapsed % 60);
    }
    canvas_draw_str_aligned(canvas, 0, 44, AlignLeft, AlignTop, furi_string_get_cstr(str));

    elements_button_center(canvas, "Stop");

    furi_string_free(str);
}

// Stopping asks for a confirmation, Back and OK both lead to it
static bool thermometer_scene_log_input_callback(InputEvent* event, void* context) {
    Thermometer* app = context;
    bool consumed = false;
    bool stop = false;

    if(event->type == InputTypeShort) {
        with_view_model(
            app->log_view,
            ThermometerLogViewModel * model,
            {
                if(!model->confirm_stop) {
                    if(event->key == InputKeyOk || event->key == InputKeyBack) {
                        model->confirm_stop = true;
                        consumed = true;
                    }
                } else if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    stop = true;
                    consumed = true;
                } else if(event->key == InputKeyBack || event->key == InputKeyLeft) {
                    model->confirm_stop = false;
                    consumed = true;
                }
            },
            consumed);
    } else if(event->key == InputKeyBack) {
        // Swallow the other Back events, the confirmation dialog handles it
        consumed = true;
    }

    if(stop) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ThermometerCustomEventLogStop);
    }

    return consumed;
}

View* thermometer_scene_log_view_alloc(Thermometer* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(ThermometerLogViewModel));
    view_set_context(view, app);
    view_set_draw_callback(view, thermometer_scene_log_draw_callback);
    view_set_input_callback(view, thermometer_scene_log_input_callback);
    return view;
}

void thermometer_scene_log_view_free(View* view) {
    view_free(view);
}

static void thermometer_scene_log_update(Thermometer* app) {
    const uint32_t elapsed = furi_hal_rtc_get_timestamp() - app->log_start_timestamp;
    const char* path = furi_string_get_cstr(app->log_path);
    const char* name = strrchr(path, '/');

    with_view_model(
        app->log_view,
        ThermometerLogViewModel * model,
        {
            strlcpy(model->file_name, name ? name + 1 : path, sizeof(model->file_name));
            model->has_value = app->log_has_value;
            model->value = app->log_last_value;
            model->rows = app->log_rows;
            model->errors = app->log_errors;
            model->elapsed = elapsed;
            model->write_failed = app->log_write_failed;
        },
        true);
}

// Open for append and close after every row: a crash loses one row at most
static bool thermometer_scene_log_append(Thermometer* app, const char* line) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool success = false;
    if(storage_file_open(file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
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

    // A restart within the same second must not append to the previous file
    FuriString* base = furi_string_alloc_set(app->log_path);
    furi_string_replace(base, ".csv", "");
    for(uint32_t n = 1; storage_file_exists(storage, furi_string_get_cstr(app->log_path)); n++) {
        furi_string_printf(app->log_path, "%s-%lu.csv", furi_string_get_cstr(base), n);
    }
    furi_string_free(base);
    furi_record_close(RECORD_STORAGE);

    app->log_write_failed = !thermometer_scene_log_append(app, LOG_HEADER);

    with_view_model(
        app->log_view, ThermometerLogViewModel * model, { model->confirm_stop = false; }, false);
    thermometer_scene_log_update(app);
    thermometer_status_publish(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewLog);

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
        case ThermometerCustomEventLogStop:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case OneWireWorkerEventReadStart:
            thermometer_notification_message(app, ThermometerNotificationMessageReadStart);
            consumed = true;
            break;
        case OneWireWorkerEventReadSuccess:
            thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
            thermometer_scene_log_write_row(app);
            thermometer_scene_log_update(app);
            thermometer_status_publish(app, true);
            consumed = true;
            break;
        case OneWireWorkerEventReadError:
            thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
            app->log_errors++;
            thermometer_notification_message(app, ThermometerNotificationMessageError);
            thermometer_scene_log_update(app);
            thermometer_status_publish(app, false);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        thermometer_scene_log_update(app);
        thermometer_status_publish(app, false);
    }

    return consumed;
}

void thermometer_scene_log_on_exit(void* context) {
    Thermometer* app = context;

    onewire_worker_stop(app->onewire_worker);
    app->log_active = false;
    thermometer_status_publish(app, false);

    thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
}
