#include "thermometer.h"
#include "onewire/onewire_worker.h"

#include <furi.h>
#include <dolphin/dolphin.h>

#include <thermometer_icons.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>

#include <lib/one_wire/one_wire_host.h>

#include "scenes/thermometer_scene.h"

#define TM_APP_VERSION "0.1.3"

struct Thermometer {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    Popup* popup;

    OneWireWorker* onewire_worker;
    OneWireDeviceData device;
};

typedef enum {
    ThermometerViewSubmenu,
    ThermometerViewWidget,
    ThermometerViewPopup,
} ThermometerView;
