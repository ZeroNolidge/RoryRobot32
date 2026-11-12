#include "display.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

namespace {
Arduino_DataBus* bus = nullptr;
Arduino_GFX* gfx = nullptr;
uint16_t* frame = nullptr;
Raster raster;
float blNow = 0, blTarget = 0;
const int kBlBits = 10;
}  // namespace

namespace Display {

bool begin() {
  bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, GFX_NOT_DEFINED);
  gfx = new Arduino_ST7789(bus, PIN_LCD_RST, 0 /* rotation */, true /* IPS */, 240, 240);
  if (!gfx->begin(80000000)) return false;
  gfx->fillScreen(Raster::rgb(COL_SCREEN));

  // One full-screen frame (115 KB) in PSRAM.
  frame = (uint16_t*)heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!frame) frame = (uint16_t*)malloc(240 * 240 * 2);
  if (!frame) return false;
  raster = Raster(frame, 240, 240);

  ledcAttach(PIN_LCD_BL, 5000, kBlBits);
  ledcWrite(PIN_LCD_BL, 0);
  return true;
}

Raster& canvas() { return raster; }

void present() { gfx->draw16bitRGBBitmap(0, 0, frame, 240, 240); }

void setBacklight(int percent) { blTarget = constrain(percent, 0, 100); }

void update() {
  blNow += (blTarget - blNow) * 0.08f;
  if (fabsf(blTarget - blNow) < 0.3f) blNow = blTarget;
  // perceptual curve so dimming looks smooth
  float f = blNow / 100.0f;
  ledcWrite(PIN_LCD_BL, (uint32_t)(f * f * ((1 << kBlBits) - 1)));
}

}  // namespace Display
