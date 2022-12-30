#include "../thermometer_i.h"

typedef enum {
    SubmenuIndexThermometerRead,
    SubmenuIndexThermometerInfo,
    SubmenuIndexThermometerAbout,
} SubmenuIndex;

void thermometer_scene_start_submenu_callback(void* context, uint32_t index) {
    Thermometer* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void thermometer_scene_start_on_enter(void* context) {
    Thermometer* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Read",
        SubmenuIndexThermometerRead,
        thermometer_scene_start_submenu_callback,
        app);

    if(onewire_device_is_device_read(&app->device)) {
        submenu_add_item(
            submenu,
            "Info",
            SubmenuIndexThermometerInfo,
            thermometer_scene_start_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu,
        "About",
        SubmenuIndexThermometerAbout,
        thermometer_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, ThermometerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewSubmenu);
}

bool thermometer_scene_start_on_event(void* context, SceneManagerEvent event) {
    Thermometer* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexThermometerRead:
            scene_manager_next_scene(app->scene_manager, ThermometerSceneRead);
            consumed = true;
            break;
        case SubmenuIndexThermometerInfo:
            scene_manager_next_scene(app->scene_manager, ThermometerSceneInfo);
            consumed = true;
            break;
        case SubmenuIndexThermometerAbout:
            scene_manager_next_scene(app->scene_manager, ThermometerSceneAbout);
            consumed = true;
            break;
        default:
            scene_manager_set_scene_state(app->scene_manager, ThermometerSceneStart, event.event);
            break;
        }
    }

    return consumed;
}

void thermometer_scene_start_on_exit(void* context) {
    Thermometer* app = context;
    submenu_reset(app->submenu);
}
