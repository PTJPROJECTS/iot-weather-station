#ifndef CONFIG_H
#define CONFIG_H

// Put real credentials in src/secrets.h (gitignored), same macro names.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-password"
#endif
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.10"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef STATION_ID
#define STATION_ID "wx01"
#endif

#define READ_INTERVAL_MS 30000UL

#endif
