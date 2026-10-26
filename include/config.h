#pragma once

#include <Arduino.h>

// =====================================================================================
// PANEL TOPOLOGY
// =====================================================================================
// Set to 1 if each lab uses TWO chained 64x64 panels (4 panels total across
// both labs). Set to 0 if each lab uses ONE native 128x64 panel (2 panels total
// across both labs).
#if USE_64X64_QUAD_PANELS
#define PANEL_RES_X 64
#define PANELS_PER_LAB 2
#else
// Native 128x64 panels (2 panels total, 1 per lab)
#define PANEL_RES_X 128
#define PANELS_PER_LAB 1
#endif
#define PANEL_RES_Y 64

// Total chain: both labs' panels daisy-chained together.
#define PANEL_CHAIN_LEN (PANELS_PER_LAB * 2)

#define LAB_WIDTH (PANEL_RES_X * PANELS_PER_LAB) // 128
#define LAB_HEIGHT PANEL_RES_Y                   // 64
#define TOTAL_WIDTH (LAB_WIDTH * 2)              // 256
#define TOTAL_HEIGHT LAB_HEIGHT                  // 64

// Viewport X-offsets for each lab within the combined virtual canvas.
#define LAB1_X_OFFSET 0
#define LAB2_X_OFFSET LAB_WIDTH

// Clock screen text. The header is identical scrolling text on both panels; the
// footer is each panel's own lab label. The two footer texts are only the defaults —
// each lab's name can be renamed from the web dashboard (stored in NVS, see LabNames).
#define HEADER_MARQUEE_TEXT "INFORMATION TECHNOLOGY"
#define HEADER_MARQUEE_SPEED_PX_PER_SEC 25
#define LAB1_FOOTER_TEXT "COMLAB 1"
#define LAB2_FOOTER_TEXT "COMLAB 2"
// Size-2 footer font is 12px per char, so 10 chars fill a 128px panel; 8 or fewer keeps the
// name clear of the GPS/RTC status dots in the footer's top-right corner.
#define LAB_NAME_MAX_LEN 10

// =====================================================================================
// GPIO PINOUT — classic ESP32 WROOM-32 (NOT the S3 variant this project started
// on)
// =====================================================================================
// HUB75 pins below are this board's actual wiring, given as-is. Everything else
// (speakers/RTC/GPS/buttons) had to be fitted into whatever classic-ESP32 GPIOs
// remained, under real hardware constraints a WROOM-32 imposes that didn't
// apply to the S3 build:
//   - GPIO 6-11 are wired to the internal SPI flash and are never usable for
//   anything else.
//   - GPIO 1/3 are UART0 TX/RX (Serial + the programming port) — left free of
//   peripherals.
//   - GPIO 34/35/36/39 are INPUT-ONLY: no internal pull-up/pull-down at all,
//   and they
//     cannot drive an output (so they can't drive a speaker).
//   - GPIO 0/2/15 are boot-strapping pins (sampled for boot mode at reset).
// After placing the 14 HUB75 pins, only {0, 2, 16, 21, 22, 34, 35, 36, 39} were
// left, plus 17 (freed by moving GPS RX onto an input-only pin). That's just
// barely enough for 2 speaker outputs + I2C + UART + 4 buttons — see the IMPORTANT
// notes below.

// --- HUB75 Matrix ---
#define PIN_R1 25
#define PIN_G1 26
#define PIN_B1 27
#define PIN_R2 14
#define PIN_G2 12
#define PIN_B2 13
#define PIN_A 19
#define PIN_B 5
#define PIN_C 33
#define PIN_D 18
#define PIN_E 23 // needed for 64-row scan panels (1/32 scan)
#define PIN_LAT 15
#define PIN_OE 32
#define PIN_CLK 4

// --- Speakers (PAM8403 stereo amplifier: L = Comlab 1, R = Comlab 2) ---
// Each pin outputs a PWM tone. Wire each one through a divider so the 3.3V signal doesn't
// clip the amp's input (it reaches full output at well under 1V):
//   GPIO --[10k]--+-- PAM8403 L (or R) input
//                 +--[1k]-- GND      (optional 10nF from input to GND smooths the tone)
// and tie the PAM8403's GND to ESP32 GND. The 1k to GND also holds GPIO2 (a boot-strapping
// pin) LOW at reset, which is the state it needs.
#define PIN_SPEAKER_LAB1 2  // -> PAM8403 L input
#define PIN_SPEAKER_LAB2 16 // -> PAM8403 R input

// --- RTC (DS3231) I2C --- (ESP32's conventional default I2C pins, unused by
// anything else)
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// --- GPS (UART1) ---
// RX is on an input-only pin (34) — that's fine, UART RX is pure input and the
// ESP32's GPIO matrix can route it there freely. TX must be a normal GPIO since
// it's an output.
#define PIN_GPS_RX 34 // ESP32 RX <- GPS TX (input-only pin, OK for RX)
#define PIN_GPS_TX 17 // ESP32 TX -> GPS RX
#define GPS_BAUD 9600
// If no checksum-valid NMEA arrives, TimeManager cycles through these bauds on
// PIN_GPS_RX, then on PIN_GPS_TX as RX (covers TX/RX swapped at the module),
// until one works.
#define GPS_PROBE_BAUDS {9600, 38400, 115200, 57600, 4800}
#define GPS_PROBE_DWELL_MS                                                     \
  2500UL // time to listen on each baud/pin before moving on
#define GPS_LINK_LOST_MS                                                       \
  10000UL // no valid sentence this long = restart probing

// --- Buttons (active LOW) ---
// IMPORTANT: UP/DOWN/SELECT sit on input-only GPIOs (34-39 have no internal
// pull resistor at all), so `INPUT_PULLUP` in button_manager.cpp has no effect
// on them — each of these three needs an EXTERNAL ~10kOhm pull-up resistor
// to 3.3V, or the button will float and trigger randomly. BACK is on GPIO0,
// which supports the internal pull-up normally (same pin/mechanism as this
// board's own BOOT button — holding BACK while powering on will enter download
// mode instead of booting normally, a minor, well-known quirk of using GPIO0
// this way).
#define PIN_BTN_UP 35     // needs external 10k pull-up to 3.3V
#define PIN_BTN_DOWN 36   // needs external 10k pull-up to 3.3V
#define PIN_BTN_SELECT 39 // needs external 10k pull-up to 3.3V
#define PIN_BTN_BACK 0    // internal pull-up works (same as BOOT button)

// =====================================================================================
// DISPLAY
// =====================================================================================
#define DEFAULT_BRIGHTNESS 255 // 1-255 (~70%)
#define MATRIX_COLOR_DEPTH_BITS 6

// =====================================================================================
// TIME / TIMEZONE
// =====================================================================================
#define DEFAULT_UTC_OFFSET_MINUTES (8 * 60) // UTC+8
#define DEFAULT_USE_12H_FORMAT true
#define RTC_RESYNC_FROM_GPS_INTERVAL_MS                                        \
  (5UL * 60UL * 1000UL) // resync RTC from GPS every 5 min when locked
// GPS counts as locked only while fixed RMC sentences keep arriving; after this
// long with none (antenna lost sky, cable unplugged) the clock falls back to
// the RTC.
#define GPS_LOCK_TIMEOUT_MS 3000UL
// No GPS bytes for this long = module not talking (wiring/baud), shown as "NO
// DATA".
#define GPS_DATA_TIMEOUT_MS 2000UL
// Rejects receiver-default dates some modules emit before they've decoded the
// almanac.
#define GPS_MIN_VALID_YEAR 2024

// =====================================================================================
// ALARMS
// =====================================================================================
#define MAX_ALARMS_PER_LAB 20
#define ALARM_LABEL_MAX_LEN 16

enum AlarmTarget : uint8_t {
  TARGET_LAB1 = 0,
  TARGET_LAB2 = 1
};

enum AlarmDayMask : uint8_t {
  DAY_SUN = 1 << 0,
  DAY_MON = 1 << 1,
  DAY_TUE = 1 << 2,
  DAY_WED = 1 << 3,
  DAY_THU = 1 << 4,
  DAY_FRI = 1 << 5,
  DAY_SAT = 1 << 6
};

#define DAYMASK_DAILY                                                          \
  (DAY_SUN | DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI | DAY_SAT)
#define DAYMASK_WEEKDAYS (DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI)

enum ChimeStyle : uint8_t {
  CHIME_WESTMINSTER = 0,
  CHIME_HIGH_FREQ_BELL = 1,
  CHIME_STANDARD_PULSE = 2,
  CHIME_DOUBLE_CHIME = 3,
  CHIME_STYLE_COUNT = 4
};

// 0 = ring until dismissed
#define ALARM_DURATION_OPTIONS_SEC {0, 5, 10, 15, 30}
#define ALARM_DURATION_OPTIONS_COUNT 5

// =====================================================================================
// SPEAKERS (PAM8403)
// =====================================================================================
// Speaker tones are ESP32 LEDC PWM square waves at the note frequency, fed to the PAM8403.
// Volume is set by the PWM duty cycle (a narrower pulse carries less energy at the note's
// pitch); 50% duty is the loudest.
#define SPEAKER_LEDC_RESOLUTION_BITS 10 // fixed: ledcWriteTone() always uses 10 bits
// Channels 0/1 would share one LEDC timer (timer = channel / 2), and the timer sets the
// pitch, so one lab's notes would retune the other's. 0 and 2 get separate timers.
#define SPEAKER_LEDC_CHANNEL_LAB1 0
#define SPEAKER_LEDC_CHANNEL_LAB2 2
#define DEFAULT_VOLUME_PERCENT 60 // 0-100, changeable from Diagnostics (menu or web)

// =====================================================================================
// BUTTONS
// =====================================================================================
#define BUTTON_DEBOUNCE_MS 30
#define BUTTON_REPEAT_DELAY_MS 500
#define BUTTON_REPEAT_INTERVAL_MS 120
#define BUTTON_SCAN_INTERVAL_MS                                                \
  5 // background scan rate; must be well under a quick tap
#define BUTTON_EVENT_QUEUE_LEN 8

// =====================================================================================
// WEB DASHBOARD (Wi-Fi access point)
// =====================================================================================
// The ESP32 hosts its own Wi-Fi network; join it and open http://192.168.4.1 (most phones
// open it automatically through the captive-portal prompt).
#define WIFI_AP_SSID "Chronos"
#define WIFI_AP_PASSWORD "chronos123" // 8+ chars (WPA2); "" = open network
#define WIFI_AP_CHANNEL 11 // 1 and 6 are taken by nearby hotspots
#define WIFI_AP_MAX_CLIENTS 4
#define WIFI_AP_TX_POWER WIFI_POWER_11dBm // lower = less supply sag; raise for more range
#define WEB_ADMIN_USER "admin"
#define WEB_ADMIN_DEFAULT_PASSWORD "admin123" // changeable from the dashboard
#define WEB_MAX_SESSIONS 4

// =====================================================================================
// NVS
// =====================================================================================
#define NVS_NAMESPACE "chronos"
