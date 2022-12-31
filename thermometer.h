#pragma once

typedef struct Thermometer Thermometer;

typedef enum {
    ThermometerNotificationMessageError,
    ThermometerNotificationMessageSuccess,
    ThermometerNotificationMessageReadStart,
    ThermometerNotificationMessageBlinkStop,
} ThermometerNotificationMessage;

void thermometer_notification_message(
    Thermometer* thermometer,
    ThermometerNotificationMessage message);
