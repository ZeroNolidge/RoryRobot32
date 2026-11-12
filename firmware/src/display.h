// ST7789 240x240 screen + PWM backlight.
#pragma once
#include <stdint.h>
#include "raster.h"

namespace Display {
bool begin();
Raster& canvas();           // draw Rory here...
void present();             // ...then push it to the screen
void setBacklight(int percent);  // 0..100, eased
void update();              // call every frame (backlight fades)
}  // namespace Display
