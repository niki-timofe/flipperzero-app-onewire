#include "thermometer_i.h"

static const NotificationSequence* thermometer_notification_sequences[] = {
    &sequence_error,
    &sequence_success,
    &sequence_blink_start_cyan,
    &sequence_blink_stop,
};

bool thermometer_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Thermometer* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
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

Thermometer* thermometer_alloc() {
    Thermometer* app = malloc(sizeof(Thermometer));

    app->gui = furi_record_open(RECORD_GUI);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&thermometer_scene_handlers, app);
    view_dispatcher_enable_queue(app->view_dispatcher);

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

    scene_manager_next_scene(app->scene_manager, ThermometerSceneStart);

    app->onewire_worker = onewire_worker_alloc();

    return app;
}

void thermometer_app_free(Thermometer* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewSubmenu);
    submenu_free(app->submenu);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewWidget);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    onewire_worker_free(app->onewire_worker);

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

    DOLPHIN_DEED(DolphinDeedPluginStart);

    view_dispatcher_run(thermometer->view_dispatcher);

    thermometer_app_free(thermometer);

    return 0;
}
