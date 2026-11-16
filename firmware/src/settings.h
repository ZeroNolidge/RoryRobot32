// Rory's settings, read from the microSD card so nothing secret is in the code.
//
//   /rory/config.txt
//     wifi_name     = MyHomeWiFi
//     wifi_password = secret
//     server        = https://rory-server.yourname.workers.dev
//     token         = the RORY_DEVICE_TOKEN you set on the server
//     eyes          = rory-eyes      (optional: her camera board's name or address)
#pragma once
#include <Arduino.h>

namespace Settings {
struct Values {
  String wifiName, wifiPassword, server, token;
  String eyes = "rory-eyes";  // camera board: mDNS name (without .local) or an IP address
};
bool load();          // call after the SD card is mounted
const Values& get();
bool aiConfigured();  // Wi-Fi + server + token all present
}  // namespace Settings
