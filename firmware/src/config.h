// Rory the Robot - board pins, colors and tuning knobs.
// Board: Waveshare ESP32-S3-Touch-LCD-1.54 (ESP32-S3R8, 240x240 ST7789, CST816 touch,
// QMI8658 IMU, ES8311 speaker codec + ES7210 dual mics, microSD, LiPo charger).
// Pin numbers come from Waveshare's official examples for this board.
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- pins
// Display (SPI, ST7789 240x240)
#define PIN_LCD_SCLK 38
#define PIN_LCD_MOSI 39
#define PIN_LCD_CS 21
#define PIN_LCD_DC 45
#define PIN_LCD_RST 40
#define PIN_LCD_BL 46

// Shared I2C bus: touch (CST816 0x15), IMU (QMI8658 0x6B), ES8311 (0x18), ES7210 (0x40)
#define PIN_I2C_SDA 42
#define PIN_I2C_SCL 41
#define PIN_TOUCH_RST 47
#define PIN_TOUCH_INT 48

// Audio (I2S to ES8311 codec -> NS4150B amp -> speaker)
#define PIN_I2S_MCLK 8
#define PIN_I2S_BCLK 9
#define PIN_I2S_LRCK 10
#define PIN_I2S_DOUT 12
#define PIN_I2S_DIN 11  // from ES7210 mics (used in the AI phase)
#define PIN_AMP_EN 7

// microSD (SDMMC 4-bit)
#define PIN_SD_CLK 16
#define PIN_SD_CMD 15
#define PIN_SD_D0 17
#define PIN_SD_D1 18
#define PIN_SD_D2 13
#define PIN_SD_D3 14

// Power & buttons
#define PIN_BAT_HOLD 2   // must be driven HIGH to stay powered on battery
#define PIN_BAT_ADC 1    // battery voltage / 3
#define PIN_CHARGING 3   // LOW while charging
#define PIN_BTN_BOOT 0   // "BOOT" button (active low)
#define PIN_BTN_PWR 5    // "PWR" button (active low)

// ---------------------------------------------------------------- Rory's colors
// Sampled from Rory in the Talk Trail app.
#define COL_SCREEN 0x263238   // dark face screen
#define COL_SPECKLE 0x303E45  // felt texture dots
#define COL_EYE 0x52DAE8      // cyan eyes + smile
#define COL_WHITE 0xFFFFFF    // eye outline + sparkle
#define COL_MOUTH 0xC62828    // open mouth
#define COL_TONGUE 0xFF8A80   // tongue
#define COL_STAR 0xF9E53A     // yellow (chest button) - cheer sparkles
#define COL_HEART 0xFF6B81    // love eyes

// ---------------------------------------------------------------- behavior tuning
#define FRAME_MS 33               // ~30 fps
#define SLEEPY_AFTER_MS (3UL * 60 * 1000)   // no play for 3 min -> yawns
#define ASLEEP_AFTER_MS (20UL * 1000)       // ...then falls asleep 20 s later
#define POWER_OFF_AFTER_MS (30UL * 60 * 1000)  // asleep 30 min -> power off (battery only)
#define BACKLIGHT_AWAKE 100       // percent
#define BACKLIGHT_ASLEEP 12       // percent

// Speaker volume steps (0..21 in the audio library). BOOT button cycles them.
// Kept deliberately modest for little ears.
static const uint8_t VOLUME_STEPS[] = {8, 13, 17};
#define VOLUME_DEFAULT_STEP 1

// ---------------------------------------------------------------- sensor orientation
// If Rory's eyes look the wrong way when you tilt her, flip these.
#define IMU_LOOK_X_SIGN (+1.0f)
#define IMU_LOOK_Y_SIGN (-1.0f)
// Which accelerometer axis points out of the screen, and its sign.
#define IMU_SCREEN_AXIS 2         // 0=x 1=y 2=z
#define IMU_SCREEN_SIGN (+1.0f)
// Print motion sensor values over USB serial (for tuning the thresholds in senses.cpp)
#define MOTION_DEBUG 0

// Touch panel orientation fixes
#define TOUCH_SWAP_XY false
#define TOUCH_MIRROR_X false
#define TOUCH_MIRROR_Y false
