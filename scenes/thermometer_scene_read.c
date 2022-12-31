#include "../thermometer_i.h"

void onewire_worker_on_event(OneWireWorkerEvent event, void* context) {
    UNUSED(event);

    Thermometer* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void thermometer_scene_read_on_enter(void* context) {
    Thermometer* app = context;

    Popup* popup = app->popup;

    popup_set_header(popup, "1-Wire", 95, 26, AlignCenter, AlignBottom);
    popup_set_text(popup, "Waiting\nfor device ...", 95, 30, AlignCenter, AlignTop);
    popup_set_icon(popup, 0, 5, &I_DolphinWait_61x59);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewPopup);

    thermometer_notification_message(app, ThermometerNotificationMessageReadStart);

    onewire_worker_start(
        app->onewire_worker, OneWireWorkerSearch, &app->device, onewire_worker_on_event, context);
}

bool thermometer_scene_read_on_event(void* context, SceneManagerEvent event) {
    Thermometer* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case OneWireWorkerEventReadSuccess:
            thermometer_notification_message(app, ThermometerNotificationMessageSuccess);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, ThermometerSceneInfo);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void thermometer_scene_read_on_exit(void* context) {
    Thermometer* app = context;

    onewire_worker_stop(app->onewire_worker);

    thermometer_notification_message(app, ThermometerNotificationMessageBlinkStop);
}
