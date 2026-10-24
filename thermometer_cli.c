#include "thermometer_cli.h"

#include <cli/cli.h>
#include <toolbox/args.h>

#define TM_CLI_COMMAND      "thermometer"
#define TM_CLI_READ_TIMEOUT 30000
#define TM_CLI_POLL_MS      100

static void thermometer_cli_print_usage(void) {
    printf("Usage:\r\n");
    printf(TM_CLI_COMMAND " status     - logging state and last reading\r\n");
    printf(TM_CLI_COMMAND " read       - search for a sensor and read it once\r\n");
    printf(TM_CLI_COMMAND " info       - data of the last read sensor\r\n");
    printf(TM_CLI_COMMAND " log start  - start logging to CSV\r\n");
    printf(TM_CLI_COMMAND " log stop   - stop logging\r\n");
    printf(TM_CLI_COMMAND " clear      - delete all log files\r\n");
}

static uint32_t thermometer_cli_scene(Thermometer* app) {
    return scene_manager_get_current_scene(app->scene_manager);
}

// Read and log are available from the start screen and after a read, like in the menu
static bool thermometer_cli_is_idle(Thermometer* app) {
    const uint32_t scene = thermometer_cli_scene(app);
    return scene == ThermometerSceneStart || scene == ThermometerSceneInfo;
}

static void thermometer_cli_print_device(Thermometer* app) {
    // Snapshot: the worker may update the device while we print
    OneWireDeviceData device = app->device;
    const OneWireDeviceReader* reader = onewire_device_reader_registry_get_by_family(
        &onewire_device_reader_registry, device.address[0]);
    OneWireDeviceDataParsed* parsed = reader->parse(&device);

    printf("sensor: %s\r\n", reader->family_name);
    printf("addr: ");
    for(size_t i = 0; i < COUNT_OF(device.address); i++) {
        printf("%02X", device.address[i]);
    }
    printf("\r\n");
    // Only the log worker stamps readings
    if(device.timestamp) printf("unix_ts: %lu\r\n", device.timestamp);
    printf("raw: %d\r\n", parsed->raw);
    printf("temp_c: %.4f\r\n", parsed->value);

    free(parsed);
}

static void thermometer_cli_status(Thermometer* app) {
    printf("logging: %s\r\n", app->log_active ? "yes" : "no");
    if(!app->log_active) return;

    printf("file: %s\r\n", furi_string_get_cstr(app->log_path));
    printf("rows: %lu\r\n", app->log_rows);
    printf("errors: %lu\r\n", app->log_errors);
    printf("elapsed_s: %lu\r\n", furi_hal_rtc_get_timestamp() - app->log_start_timestamp);

    if(app->log_has_value) {
        thermometer_cli_print_device(app);
    } else {
        printf("temp_c: none\r\n");
    }
}

static void thermometer_cli_info(Thermometer* app) {
    if(!onewire_device_is_device_read(&app->device)) {
        printf("error: no sensor read yet, run \"" TM_CLI_COMMAND " read\"\r\n");
        return;
    }
    thermometer_cli_print_device(app);
}

static void thermometer_cli_read(Thermometer* app) {
    if(!thermometer_cli_is_idle(app)) {
        printf("error: busy, stop logging or leave the current screen first\r\n");
        return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, ThermometerCustomEventCliRead);

    // The read scene switches to the info scene on success
    uint32_t waited = 0;
    while(thermometer_cli_scene(app) != ThermometerSceneInfo) {
        if(waited >= TM_CLI_READ_TIMEOUT) {
            printf("error: no sensor found in %d s\r\n", TM_CLI_READ_TIMEOUT / 1000);
            return;
        }
        furi_delay_ms(TM_CLI_POLL_MS);
        waited += TM_CLI_POLL_MS;
    }
    thermometer_cli_print_device(app);
}

static void thermometer_cli_log(Thermometer* app, FuriString* args) {
    FuriString* action = furi_string_alloc();
    args_read_string_and_trim(args, action);

    if(furi_string_equal(action, "start")) {
        if(app->log_active) {
            printf("error: already logging\r\n");
        } else if(!thermometer_cli_is_idle(app)) {
            printf("error: busy, leave the current screen first\r\n");
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ThermometerCustomEventCliLogStart);
            printf("logging started\r\n");
        }
    } else if(furi_string_equal(action, "stop")) {
        if(!app->log_active) {
            printf("error: not logging\r\n");
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ThermometerCustomEventCliLogStop);
            printf("logging stopped\r\n");
        }
    } else {
        thermometer_cli_print_usage();
    }

    furi_string_free(action);
}

static void thermometer_cli_clear(Thermometer* app) {
    if(app->log_active) {
        printf("error: logging, stop it first\r\n");
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = storage_simply_remove_recursive(storage, TM_LOG_DIR);
    furi_record_close(RECORD_STORAGE);

    printf(ok ? "logs deleted\r\n" : "error: delete failed\r\n");
}

static void thermometer_cli_execute(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    Thermometer* app = context;

    FuriString* cmd = furi_string_alloc();
    if(!args_read_string_and_trim(args, cmd)) {
        thermometer_cli_print_usage();
    } else if(furi_string_equal(cmd, "status")) {
        thermometer_cli_status(app);
    } else if(furi_string_equal(cmd, "read")) {
        thermometer_cli_read(app);
    } else if(furi_string_equal(cmd, "info")) {
        thermometer_cli_info(app);
    } else if(furi_string_equal(cmd, "log")) {
        thermometer_cli_log(app, args);
    } else if(furi_string_equal(cmd, "clear")) {
        thermometer_cli_clear(app);
    } else {
        thermometer_cli_print_usage();
    }
    furi_string_free(cmd);
}

void thermometer_cli_register(Thermometer* app) {
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        registry, TM_CLI_COMMAND, CliCommandFlagParallelSafe, thermometer_cli_execute, app);
    furi_record_close(RECORD_CLI);
}

void thermometer_cli_unregister(Thermometer* app) {
    UNUSED(app);
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(registry, TM_CLI_COMMAND);
    furi_record_close(RECORD_CLI);
}
