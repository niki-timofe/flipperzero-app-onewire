#include "../thermometer_i.h"

typedef enum {
    ClearLogsEventCancel,
    ClearLogsEventDelete,
} ClearLogsEvent;

static void thermometer_scene_clear_logs_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    Thermometer* app = context;

    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ClearLogsEventCancel);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ClearLogsEventDelete);
    }
}

void thermometer_scene_clear_logs_on_enter(void* context) {
    Thermometer* app = context;
    Widget* widget = app->widget;

    widget_add_string_multiline_element(
        widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Delete all\nlog files?");
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", thermometer_scene_clear_logs_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Delete", thermometer_scene_clear_logs_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewWidget);
}

bool thermometer_scene_clear_logs_on_event(void* context, SceneManagerEvent event) {
    Thermometer* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ClearLogsEventDelete) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            const bool ok = storage_simply_remove_recursive(storage, TM_LOG_DIR);
            furi_record_close(RECORD_STORAGE);

            notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
        }
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void thermometer_scene_clear_logs_on_exit(void* context) {
    Thermometer* app = context;
    widget_reset(app->widget);
}
