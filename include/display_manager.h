#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Preferences.h>
#include "config.h"

// Data needed to render one lab's clock viewport. Layout is 3 stacked sections:
// header marquee (top, size 2, yellow, scrolling, identical on both panels) /
// big time (middle, size 4, red) / footer (bottom, size 2, blue, each panel's own label).
struct LabScreenData {
  const char *timeHM;        // "12:30" (no seconds — doesn't fit at size 4 on 128px wide)
  const char *footerText;    // "COMLAB 1" / "COMLAB 2"
  bool gpsLocked;
  bool rtcPresent;
};

// Wraps the ESP32-HUB75-MatrixPanel-DMA driver and draws the two independent lab viewports
// side-by-side on the single chained virtual canvas.
class DisplayManager {
public:
  bool begin();

  void setBrightness(uint8_t brightness); // 1-255, persists to NVS immediately
  uint8_t getBrightness() const { return currentBrightness_; }
  // Thread-safe: queues a brightness change that the display task applies in its next show().
  // The web task must use this, not setBrightness(), because the library rewrites the DMA
  // buffer when brightness changes, which would race the display task drawing into it.
  void requestBrightness(uint8_t brightness) { pendingBrightness_ = brightness; }
  void clear(); // clears the off-screen frame (not the panel itself)
  void show();  // pushes only the pixels that changed since the last show() to the panel

  void drawClockScreen(uint8_t labIndex, const LabScreenData &data);
  void drawAlarmActive(uint8_t labIndex, const char *bannerText, bool flashOn);

  // Generic scrollable menu list rendering for one lab viewport. Since both labs share the
  // one physical keypad, callers draw the same menu on both (labIndex 0 and 1) each frame.
  void drawMenuLines(uint8_t labIndex, const char *title, const char *const *lines,
                      uint8_t lineCount, int8_t selectedLine);

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const;

private:
  MatrixPanel_I2S_DMA *matrix_ = nullptr;
  // The DMA buffer is single-buffered (no RAM for two on a classic ESP32) and is scanned out
  // continuously, so clearing + redrawing it directly shows half-drawn frames as flicker.
  // Instead every frame is composed in back_ (normal heap), and show() rewrites only the rows
  // whose content hash changed since the last push. Rewriting a pixel with its current color
  // is invisible, so each LED still goes straight from its old color to its new one. A hash
  // per row (256 B) replaces a full 32 KB copy of the panel: that RAM is DMA-capable, and
  // Wi-Fi needs it for packet buffers (without it clients associate but never get DHCP).
  GFXcanvas16 *back_ = nullptr;
  uint32_t rowHash_[TOTAL_HEIGHT] = {};
  Adafruit_GFX *gfx_ = nullptr; // back_ if allocated, else matrix_ (direct, flickery fallback)
  Preferences prefs_;
  uint8_t currentBrightness_ = DEFAULT_BRIGHTNESS;
  volatile int16_t pendingBrightness_ = -1; // -1 = none; see requestBrightness()
  int16_t labOffsetX(uint8_t labIndex) const {
    return labIndex == 0 ? LAB1_X_OFFSET : LAB2_X_OFFSET;
  }
};
