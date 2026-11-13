#include "settings.h"
#include <FS.h>
#include <SD_MMC.h>

namespace {
Settings::Values values;
}

namespace Settings {

bool load() {
  File f = SD_MMC.open("/rory/config.txt");
  if (!f) {
    Serial.println("[settings] no /rory/config.txt - Rory will play offline");
    return false;
  }
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq), val = line.substring(eq + 1);
    key.trim();
    val.trim();
    key.toLowerCase();
    if (key == "wifi_name") values.wifiName = val;
    else if (key == "wifi_password") values.wifiPassword = val;
    else if (key == "server") values.server = val;
    else if (key == "token") values.token = val;
  }
  f.close();
  while (values.server.endsWith("/")) values.server.remove(values.server.length() - 1);
  Serial.printf("[settings] Wi-Fi \"%s\", server %s, token %s\n", values.wifiName.c_str(),
                values.server.isEmpty() ? "(none)" : values.server.c_str(), values.token.isEmpty() ? "missing" : "set");
  return true;
}

const Values& get() { return values; }

bool aiConfigured() {
  return !values.wifiName.isEmpty() && values.server.startsWith("https://") && !values.token.isEmpty();
}

}  // namespace Settings
