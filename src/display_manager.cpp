#include "display_manager.h"

bool DisplayManager::begin() {
  HUB75_I2S_CFG::i2s_pins pins = {
    PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
    PIN_A, PIN_B, PIN_C, PIN_D, PIN_E,
    PIN_LAT, PIN_OE, PIN_CLK
  };

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN_LEN,
    pins
  );
  mxconfig.clkphase = false;
  // A 256x64 canvas at the library's 8-bit default color depth, double-buffered, doesn't
  // fit in a classic ESP32's ~300KB internal DMA-capable RAM (no PSRAM on this board).
  // Single-buffered + reduced color depth cuts that to a quarter. Tear-free updates are
  // instead handled by the off-screen frame + diff push in show() (see display_manager.h).
  mxconfig.double_buff = false;
  mxconfig.setPixelColorDepthBits(MATRIX_COLOR_DEPTH_BITS);

  matrix_ = new MatrixPanel_I2S_DMA(mxconfig);
  bool ok = matrix_->begin();

  prefs_.begin(NVS_NAMESPACE, false);
  currentBrightness_ = prefs_.getUChar("brightness", DEFAULT_BRIGHTNESS);
  prefs_.end();
  matrix_->setBrightness8(currentBrightness_);

  matrix_->clearScreen();

  back_ = new GFXcanvas16(TOTAL_WIDTH, TOTAL_HEIGHT);
  if (back_->getBuffer()) {
    gfx_ = back_;
  } else {
    Serial.println("[DisplayManager] Off-screen frame alloc failed, drawing direct (may flicker)");
    delete back_;
    back_ = nullptr;
    gfx_ = matrix_;
  }
  return ok;
}

void DisplayManager::setBrightness(uint8_t brightness) {
  currentBrightness_ = brightness;
  matrix_->setBrightness8(brightness);
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putUChar("brightness", brightness);
  prefs_.end();
}

void DisplayManager::clear() {
  if (back_) back_->fillScreen(0);
  else matrix_->clearScreen();
}

void DisplayManager::show() {
  int16_t pending = pendingBrightness_;
  if (pending >= 0) {
    pendingBrightness_ = -1;
    setBrightness((uint8_t)pending);
  }
  if (!back_) return;
  const uint16_t *src = back_->getBuffer();
  for (int16_t y = 0; y < TOTAL_HEIGHT; y++) {
    const uint16_t *row = src + (size_t)y * TOTAL_WIDTH;
    uint32_t h = 2166136261u; // FNV-1a; rowHash_ starts at 0, so the first push draws all
    for (int16_t x = 0; x < TOTAL_WIDTH; x++) h = (h ^ row[x]) * 16777619u;
    if (h == rowHash_[y]) continue;
    rowHash_[y] = h;
    for (int16_t x = 0; x < TOTAL_WIDTH; x++) matrix_->drawPixel(x, y, row[x]);
  }
}

uint16_t DisplayManager::color565(uint8_t r, uint8_t g, uint8_t b) const {
  return matrix_->color565(r, g, b);
}

// Prints `text` horizontally centered within [ox, ox+LAB_WIDTH) at the given top y, in the
// given size/color. Clamps to the left edge of the viewport so an oversized string never
// bleeds into the neighboring lab's viewport.
static void printCentered(Adafruit_GFX *m, int16_t ox, int16_t y, uint8_t size,
                           uint16_t color, const char *text) {
  m->setTextSize(size);
  int16_t bx, by;
  uint16_t bw, bh;
  m->getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
  int16_t x = ox + (LAB_WIDTH - (int16_t)bw) / 2;
  if (x < ox) x = ox;
  m->setTextColor(color);
  m->setCursor(x, y);
  m->print(text);
}

// Scrolls `text` right-to-left across [ox, ox+LAB_WIDTH) forever, driven purely by millis()
// (no persistent per-call state needed). Draws character-by-character and skips any glyph
// cell that isn't ENTIRELY inside the viewport — unlike printCentered, a marquee needs to
// travel past both edges by design, and the underlying matrix has no per-viewport clip
// rect, so without this a scrolling character could bleed pixels onto the *other* lab's
// panel while it's mid-transit across this one's edge.
static void printMarquee(Adafruit_GFX *m, int16_t ox, int16_t y, uint8_t size,
                          uint16_t color, const char *text, uint16_t speedPxPerSec) {
  m->setTextSize(size);
  int16_t bx, by;
  uint16_t bw, bh;
  m->getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);

  const int16_t charW = 6 * size; // standard GFX font cell: 5px glyph + 1px gap, scaled
  const int32_t totalRange = (int32_t)bw + LAB_WIDTH;
  const uint64_t traveled64 = ((uint64_t)millis() * speedPxPerSec / 1000) % (uint64_t)totalRange;
  int16_t cx = ox + LAB_WIDTH - (int16_t)traveled64;

  for (const char *p = text; *p; p++) {
    if (cx >= ox && (cx + charW) <= (ox + LAB_WIDTH)) {
      m->drawChar(cx, y, *p, color, color, size); // fg==bg draws transparently, like print()
    }
    cx += charW;
  }
}

void DisplayManager::drawClockScreen(uint8_t labIndex, const LabScreenData &data) {
  int16_t ox = labOffsetX(labIndex);

  gfx_->fillRect(ox, 0, LAB_WIDTH, LAB_HEIGHT, matrix_->color565(0, 0, 0));

  // Top: scrolling header (size 2, yellow), identical text on both panels.
  printMarquee(gfx_, ox, 0, 2, matrix_->color565(255, 255, 0), HEADER_MARQUEE_TEXT,
               HEADER_MARQUEE_SPEED_PX_PER_SEC);

  // Middle: big HH:MM (size 4, green), with a divider line above and below. Glyphs occupy
  // rows 0-13 (header), 17-44 (time), 48-61 (footer), so the dividers at y=15 and y=46 each
  // keep a 1px gap on both sides.
  const uint16_t dividerColor = matrix_->color565(80, 80, 80);
  gfx_->drawFastHLine(ox, 15, LAB_WIDTH, dividerColor);
  printCentered(gfx_, ox, 17, 4, matrix_->color565(0, 255, 0), data.timeHM);
  gfx_->drawFastHLine(ox, 46, LAB_WIDTH, dividerColor);

  // Bottom: footer / lab name (size 2, blue), with GPS/RTC status dots in its top-right
  // corner — the header row is now a continuously scrolling marquee, so status dots parked
  // there would flicker in and out as text passes over them.
  printCentered(gfx_, ox, 48, 2, matrix_->color565(0, 100, 255), data.footerText);

  uint16_t gpsColor = data.gpsLocked ? matrix_->color565(0, 255, 0) : matrix_->color565(80, 80, 80);
  uint16_t rtcColor = data.rtcPresent ? matrix_->color565(0, 180, 255) : matrix_->color565(120, 0, 0);
  gfx_->fillCircle(ox + LAB_WIDTH - 6, 50, 2, gpsColor);
  gfx_->fillCircle(ox + LAB_WIDTH - 14, 50, 2, rtcColor);
}

void DisplayManager::drawAlarmActive(uint8_t labIndex, const char *bannerText, bool flashOn) {
  int16_t ox = labOffsetX(labIndex);
  uint16_t bg = flashOn ? matrix_->color565(180, 0, 0) : matrix_->color565(0, 0, 0);
  uint16_t fg = flashOn ? matrix_->color565(255, 255, 255) : matrix_->color565(255, 60, 60);

  gfx_->fillRect(ox, 0, LAB_WIDTH, LAB_HEIGHT, bg);
  gfx_->drawRect(ox + 2, 2, LAB_WIDTH - 4, LAB_HEIGHT - 4, fg);

  gfx_->setTextColor(fg);
  gfx_->setTextSize(1);
  gfx_->setCursor(ox + 6, 24);
  gfx_->print("*** ALARM ***");
  gfx_->setCursor(ox + 6, 38);
  gfx_->print(bannerText);
}

void DisplayManager::drawMenuLines(uint8_t labIndex, const char *title, const char *const *lines,
                                    uint8_t lineCount, int8_t selectedLine) {
  int16_t ox = labOffsetX(labIndex);
  gfx_->fillRect(ox, 0, LAB_WIDTH, LAB_HEIGHT, matrix_->color565(0, 0, 0));

  gfx_->setTextSize(1);
  gfx_->setTextColor(matrix_->color565(255, 255, 0));
  gfx_->setCursor(ox + 2, 1);
  gfx_->print(title);
  gfx_->drawFastHLine(ox + 1, 9, LAB_WIDTH - 2, matrix_->color565(80, 80, 80));

  const int16_t listTop = 12;
  const uint8_t rowHeight = 9;
  const uint8_t maxVisibleRows = (LAB_HEIGHT - listTop) / rowHeight;

  // Scroll the window so the selected row is always visible — without this, lists longer
  // than what fits on screen (e.g. 20 alarm slots or 13 label presets vs. ~5 visible rows)
  // would let the cursor move onto rows that are never drawn.
  uint8_t scrollOffset = 0;
  if (lineCount > maxVisibleRows && selectedLine >= 0) {
    int16_t desired = (int16_t)selectedLine - maxVisibleRows / 2;
    int16_t maxOffset = (int16_t)lineCount - maxVisibleRows;
    if (desired < 0) desired = 0;
    if (desired > maxOffset) desired = maxOffset;
    scrollOffset = (uint8_t)desired;
  }

  uint8_t lastVisible = scrollOffset + maxVisibleRows;
  if (lastVisible > lineCount) lastVisible = lineCount;

  int16_t y = listTop;
  for (uint8_t i = scrollOffset; i < lastVisible; i++, y += rowHeight) {
    bool selected = (i == selectedLine);
    if (selected) {
      gfx_->fillRect(ox + 1, y - 1, LAB_WIDTH - 2, rowHeight, matrix_->color565(40, 40, 40));
      gfx_->setTextColor(matrix_->color565(0, 255, 100));
      gfx_->setCursor(ox + 2, y);
      gfx_->print(">");
    } else {
      gfx_->setTextColor(matrix_->color565(200, 200, 200));
    }
    gfx_->setCursor(ox + 9, y);
    gfx_->print(lines[i]);
  }

  // Scroll indicators so it's clear there's more content above/below.
  if (scrollOffset > 0) {
    gfx_->setTextColor(matrix_->color565(150, 150, 150));
    gfx_->setCursor(ox + LAB_WIDTH - 8, 1);
    gfx_->print("^");
  }
  if (lastVisible < lineCount) {
    gfx_->setTextColor(matrix_->color565(150, 150, 150));
    gfx_->setCursor(ox + LAB_WIDTH - 8, LAB_HEIGHT - 8);
    gfx_->print("v");
  }
}
