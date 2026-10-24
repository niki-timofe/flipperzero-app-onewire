#include "thermometer_i.h"
#include "thermometer_cli.h"

// Same green flash the firmware uses for SD card activity (storages/sd_notify.c)
static const NotificationSequence sequence_sd_write = {
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    &message_delay_50,
    NULL,
};

static const NotificationSequence* thermometer_notification_sequences[] = {
    &sequence_error,
    &sequence_success,
    &sequence_blink_start_cyan,
    &sequence_blink_stop,
    &sequence_sd_write,
};

bool thermometer_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Thermometer* app = context;
    SceneManager* scene_manager = app->scene_manager;
    const uint32_t scene = scene_manager_get_current_scene(scene_manager);

    // Same transitions as the menu items, only from the idle screens
    switch(event) {
    case ThermometerCustomEventCliRead:
        if(scene == ThermometerSceneInfo) {
            scene_manager_search_and_switch_to_previous_scene(
                scene_manager, ThermometerSceneStart);
        }
        if(scene_manager_get_current_scene(scene_manager) == ThermometerSceneStart) {
            scene_manager_next_scene(scene_manager, ThermometerSceneRead);
        }
        return true;
    case ThermometerCustomEventCliLogStart:
        if(scene == ThermometerSceneInfo) {
            scene_manager_search_and_switch_to_previous_scene(
                scene_manager, ThermometerSceneStart);
        }
        if(scene_manager_get_current_scene(scene_manager) == ThermometerSceneStart) {
            scene_manager_next_scene(scene_manager, ThermometerSceneLog);
        }
        return true;
    case ThermometerCustomEventCliLogStop:
        if(scene == ThermometerSceneLog) {
            scene_manager_search_and_switch_to_previous_scene(
                scene_manager, ThermometerSceneStart);
        }
        return true;
    default:
        return scene_manager_handle_custom_event(scene_manager, event);
    }
}

bool thermometer_back_event_callback(void* context) {
    furi_assert(context);
    Thermometer* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void thermometer_tick_event_callback(void* context) {
    furi_assert(context);
    Thermometer* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void thermometer_status_publish(Thermometer* app, bool new_reading) {
    furi_mutex_acquire(app->status_mutex, FuriWaitForever);
    ThermometerStatus* status = &app->status;
    status->log_active = app->log_active;
    strlcpy(status->log_path, furi_string_get_cstr(app->log_path), sizeof(status->log_path));
    status->log_rows = app->log_rows;
    status->log_errors = app->log_errors;
    status->log_start_timestamp = app->log_start_timestamp;
    status->log_has_value = app->log_has_value;
    if(new_reading) {
        status->device = app->device;
        status->device_valid = true;
        status->read_seq++;
    }
    furi_mutex_release(app->status_mutex);
}

void thermometer_status_get(Thermometer* app, ThermometerStatus* status) {
    furi_mutex_acquire(app->status_mutex, FuriWaitForever);
    *status = app->status;
    furi_mutex_release(app->status_mutex);
}

Thermometer* thermometer_alloc() {
    Thermometer* app = malloc(sizeof(Thermometer));
    memset(app, 0, sizeof(Thermometer));

    app->status_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->gui = furi_record_open(RECORD_GUI);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&thermometer_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, thermometer_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, thermometer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, thermometer_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ThermometerViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ThermometerViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ThermometerViewPopup, popup_get_view(app->popup));

    app->log_view = thermometer_scene_log_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ThermometerViewLog, app->log_view);

    scene_manager_next_scene(app->scene_manager, ThermometerSceneStart);

    app->onewire_worker = onewire_worker_alloc();

    app->log_path = furi_string_alloc();

    return app;
}

void thermometer_app_free(Thermometer* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewSubmenu);
    submenu_free(app->submenu);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewWidget);
    widget_free(app->widget);

    //  Popup
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewPopup);
    popup_free(app->popup);

    //  Log
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewLog);
    thermometer_scene_log_view_free(app->log_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    onewire_worker_free(app->onewire_worker);

    furi_string_free(app->log_path);
    furi_mutex_free(app->status_mutex);

    free(app);
}

void thermometer_notification_message(
    Thermometer* thermometer,
    ThermometerNotificationMessage message) {
    furi_assert(
        message < sizeof(thermometer_notification_sequences) / sizeof(NotificationSequence*));
    notification_message(thermometer->notifications, thermometer_notification_sequences[message]);
}

int32_t thermometer_app(void* p) {
    UNUSED(p);

    Thermometer* thermometer = thermometer_alloc();

    dolphin_deed(DolphinDeedPluginStart);

    thermometer_cli_register(thermometer);

    view_dispatcher_run(thermometer->view_dispatcher);

    thermometer_cli_unregister(thermometer);

    // The loader can close the app on any scene: run its on_exit (stops the worker, OTG)
    scene_manager_stop(thermometer->scene_manager);

    thermometer_app_free(thermometer);

    return 0;
}
