// Rory's settings, read from the microSD card so nothing secret is in the code.
//
//   /rory/config.txt
//     wifi_name     = MyHomeWiFi
//     wifi_password = secret
//     server        = https://rory-server.yourname.workers.dev
//     token         = the RORY_DEVICE_TOKEN you set on the server
//     eyes          = rory-eyes      (optional: her camera board's name or address)
//   Extra Wi-Fi networks (e.g. your phone's hotspot), up to 4 more:
//     wifi_name2 = My Phone          wifi_password2 = ...
//     wifi_name3 = Grandma's WiFi    wifi_password3 = ...
#pragma once
#include <Arduino.h>

namespace Settings {
struct Values {
  String wifiName, wifiPassword, server, token;
  String eyes = "rory-eyes";  // camera board: mDNS name (without .local) or an IP address
  // extra networks: wifi_name2..5 / wifi_password2..5 (index 0 = 2nd network)
  String moreNames[4], morePasswords[4];
};
bool load();          // call after the SD card is mounted
const Values& get();
bool aiConfigured();  // Wi-Fi + server + token all present
}  // namespace Settings
