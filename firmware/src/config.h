// Rory the Robot - board pins, colors and tuning knobs.
// Board: Waveshare ESP32-S3-Touch-LCD-1.54 (ESP32-S3R8, 240x240 ST7789, CST816 touch,
// QMI8658 IMU, ES8311 speaker codec + ES7210 dual mics, microSD, LiPo charger).
// Pin numbers come from Waveshare's official examples for this board.
#pragma once
#include <stdint.h>

// Your own tweaks (debug switches, timings, orientation) go in my_config.h,
// which Git ignores - updates never overwrite it. See my_config.example.h.
#if __has_include("my_config.h")
#include "my_config.h"
#endif

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
#define PIN_BTN_KEY 4    // "+/KEY" button (active low) - shows the battery gauge

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
#ifndef SLEEPY_AFTER_MS
#define SLEEPY_AFTER_MS (8UL * 60 * 1000)   // no play for 8 min -> yawns (being held counts as play)
#endif
#ifndef ASLEEP_AFTER_MS
#define ASLEEP_AFTER_MS (20UL * 1000)       // ...then falls asleep 20 s later
#endif
#ifndef POWER_OFF_AFTER_MS
#define POWER_OFF_AFTER_MS (30UL * 60 * 1000)  // asleep 30 min -> power off (battery only)
#endif
// Conversations: when Rory's answer ends with a question, she keeps listening
// without a finger on her face. She waits LISTEN_WAIT_MS for him to start, and
// stops once he's been quiet for LISTEN_QUIET_MS. (Set KEEP_LISTENING 0 to turn off.)
#ifndef KEEP_LISTENING
#define KEEP_LISTENING 1
#endif
#ifndef LISTEN_WAIT_MS
#define LISTEN_WAIT_MS 10000     // time for him to start answering
#endif
#ifndef LISTEN_QUIET_MS
#define LISTEN_QUIET_MS 2500      // he talks slowly with pauses - don't cut him off
#endif
#ifndef LISTEN_SPEECH_LEVEL
#define LISTEN_SPEECH_LEVEL 0.08f // mic level (0..1) that counts as him talking
#endif
// Held up and talked to: while he holds her (face toward him) she quietly
// checks for close, loud speech; if she hears him she shows her listening face
// and records until he pauses. Nothing is sent unless she heard him.
// Tap to talk: one tap on her face and she listens (instead of "Beep boop!").
// Holding a finger on her face still works too.
#ifndef TAP_TO_TALK
#define TAP_TO_TALK 1
#endif
#ifndef LISTEN_WHEN_HELD
#define LISTEN_WHEN_HELD 1
#endif
#ifndef HELD_SPEECH_LEVEL
#define HELD_SPEECH_LEVEL 0.12f   // a bit louder than LISTEN_SPEECH_LEVEL: he's right next to her
#endif
#ifndef HELD_SPEECH_MS
#define HELD_SPEECH_MS 300        // this much talking (not a bump or a clap) starts it
#endif
#ifndef BACKLIGHT_AWAKE
#define BACKLIGHT_AWAKE 100       // percent
#endif
#ifndef BACKLIGHT_ASLEEP
#define BACKLIGHT_ASLEEP 12       // percent
#endif

// Speaker volume steps (0..21 in the audio library). BOOT button cycles them.
// Kept deliberately modest for little ears.
static const uint8_t VOLUME_STEPS[] = {8, 13, 17};
// Loudness of streamed AI answers compared with the voice clips (raise if answers are quieter)
#ifndef STREAM_GAIN
#define STREAM_GAIN 1.0f
#endif
#ifndef VOLUME_DEFAULT_STEP
#define VOLUME_DEFAULT_STEP 1
#endif

// ---------------------------------------------------------------- sensor orientation
// If Rory's eyes look the wrong way when you tilt her, flip these.
#ifndef IMU_LOOK_X_SIGN
#define IMU_LOOK_X_SIGN (+1.0f)
#endif
#ifndef IMU_LOOK_Y_SIGN
#define IMU_LOOK_Y_SIGN (-1.0f)
#endif
// Which accelerometer axis points out of the screen, and its sign.
#ifndef IMU_SCREEN_AXIS
#define IMU_SCREEN_AXIS 2         // 0=x 1=y 2=z
#endif
#ifndef IMU_SCREEN_SIGN
#define IMU_SCREEN_SIGN (-1.0f)   // measured on a real ESP32-S3-Touch-LCD-1.54: -1 when screen faces up
#endif
// Print motion sensor values over USB serial (for tuning the thresholds in senses.cpp)
#ifndef MOTION_DEBUG
#define MOTION_DEBUG 0
#endif

// Touch panel orientation fixes
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY false
#endif
#ifndef TOUCH_MIRROR_X
#define TOUCH_MIRROR_X false
#endif
#ifndef TOUCH_MIRROR_Y
#define TOUCH_MIRROR_Y false
#endif
