#include "../thermometer_i.h"

bool thermometer_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    UNUSED(context);

    bool consumed = false;

    return consumed;
}

void thermometer_scene_about_on_enter(void* context) {
    Thermometer* app = context;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#%s\n", "Information");

    furi_string_cat_printf(temp_str, "Version: %s\n", TM_APP_VERSION);

    furi_string_cat_printf(temp_str, "\e#%s\n", "Description");
    furi_string_cat_printf(temp_str, "Reading data from various\ntemperature sensors\n\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                           \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!          Thermometer          \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewWidget);
}

void thermometer_scene_about_on_exit(void* context) {
    Thermometer* app = context;
    widget_reset(app->widget);
}
