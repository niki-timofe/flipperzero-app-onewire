#include "../thermometer_i.h"

void thermometer_scene_info_on_enter(void* context) {
    Thermometer* app = context;

    const OneWireDeviceReader* reader = onewire_device_reader_registry_get_by_family(
        &onewire_device_reader_registry, app->device.address[0]);

    FuriString* temp_str;
    temp_str = furi_string_alloc();

    furi_string_cat_printf(temp_str, "%s\n", reader->family_name);
    widget_add_string_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(temp_str));

    furi_string_reset(temp_str);

    reader->print(&app->device, temp_str);

    widget_add_string_multiline_element(
        app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewWidget);
}

bool thermometer_scene_info_on_event(void* context, SceneManagerEvent event) {
    Thermometer* app = context;
    UNUSED(app);
    UNUSED(event);

    bool consumed = false;

    return consumed;
}

void thermometer_scene_info_on_exit(void* context) {
    Thermometer* app = context;

    widget_reset(app->widget);
}
