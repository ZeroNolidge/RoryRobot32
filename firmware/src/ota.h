// Wireless updates: once Rory is on Wi-Fi she accepts new firmware from
// PlatformIO over the network (the "rory-wifi" upload), protected by her
// password (the token in /rory/config.txt).
#pragma once

namespace Ota {
void loop();  // call every loop; starts listening once Wi-Fi is up
}
