// Rory's eyes - settings.
// Your Wi-Fi details go in my_settings.h (created automatically on the first
// build from my_settings.example.h). Git ignores it, so updates never overwrite it.
#pragma once

#if __has_include("my_settings.h")
#include "my_settings.h"
#endif

#ifndef WIFI_NAME
#define WIFI_NAME "your-wifi-name"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your-wifi-password"
#endif

// The camera appears on your network as  http://rory-eyes.local
#ifndef EYES_HOSTNAME
#define EYES_HOSTNAME "rory-eyes"
#endif
