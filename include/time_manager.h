#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include "config.h"

// Precision time engine: GPS (primary, atomic UTC) with DS3231 RTC fallback/backup.
// Call begin() once, update() every loop iteration (non-blocking).
class TimeManager {
public:
  bool begin();
  void update();

  // Local time = UTC + timezoneOffsetMinutes, derived from whichever source is authoritative.
  DateTime now() const;

  bool isGpsLocked() const;
  bool isGpsReceivingData() const; // any NMEA bytes recently (false = wiring/baud/power issue)
  uint8_t satelliteCount() const;
  float gpsHdop() const;
  bool isRtcPresent() const { return rtcPresent_; } // set once in begin(), never written after

  int16_t timezoneOffsetMinutes() const;
  void setTimezoneOffsetMinutes(int16_t minutes); // persists to NVS immediately

  bool use12HourFormat() const;
  void setUse12HourFormat(bool v); // persists to NVS immediately

  // Manual set (local time as entered by the user); stored to RTC immediately.
  void setManualDateTime(const DateTime &localDt);

  // Formats "HH:MM:SS" or "HH:MM:SS AM/PM" per use12HourFormat().
  void formatTime(char *out, size_t outLen) const;
  // Formats "MON SEP 15, 2026"
  void formatDate(char *out, size_t outLen) const;
  // Splits time into a big "HH:MM" (no seconds) piece and a separate "AM"/"PM" piece (empty
  // string in 24h mode) — used for large-font clock displays too narrow to fit both together.
  void formatTimeParts(char *hm, size_t hmLen, char *ampm, size_t ampmLen) const;

  uint8_t dayOfWeek() const; // 0=Sun ... 6=Sat, local time

private:
  // mutable: TinyGPSPlus/RTC_DS3231 accessors (value()/hour()/now()/...) are non-const in
  // their libraries even though reading GPS/RTC state doesn't logically mutate this class's
  // observable state from a caller's point of view.
  mutable RTC_DS3231 rtc_;
  mutable TinyGPSPlus gps_;
  HardwareSerial gpsSerial_{1};
  Preferences prefs_;

  bool rtcPresent_ = false;
  bool gpsLocked_ = false;
  int16_t tzOffsetMinutes_ = DEFAULT_UTC_OFFSET_MINUTES;
  bool use12h_ = DEFAULT_USE_12H_FORMAT;

  unsigned long lastRtcSyncFromGpsMs_ = 0;
  bool rtcSyncedFromGps_ = false;

  // GPS time is anchored when a fixed RMC sentence arrives and advanced with millis() between
  // sentences, so the clock keeps ticking smoothly and stops being trusted once sentences stop.
  bool gpsAnchorValid_ = false;
  uint32_t gpsAnchorUnix_ = 0;
  unsigned long gpsAnchorMs_ = 0;
  unsigned long lastGpsByteMs_ = 0;
  bool gpsEverReceived_ = false;

  // Baud/pin auto-detect (see GPS_PROBE_BAUDS).
  uint8_t gpsProbeIdx_ = 0;
  bool gpsLinkFound_ = false;
  unsigned long gpsProbeStartMs_ = 0;
  uint32_t gpsProbeChecksumBase_ = 0;
  uint32_t lastPassedChecksum_ = 0;
  unsigned long lastValidSentenceMs_ = 0;
  unsigned long lastGpsLogMs_ = 0;
  int8_t gpsRxPin_ = PIN_GPS_RX;
  uint32_t gpsBaud_ = GPS_BAUD;

  // Guards gps_/rtc_/gpsLocked_/tzOffsetMinutes_/use12h_: Core 0's timeAlarmTask calls
  // update() (writes gps_ via encode(), occasionally writes rtc_ via adjust()) every loop
  // tick while Core 1's uiDisplayTask/UiMenu concurrently call now()/formatTimeParts()/etc.
  // (read gps_/rtc_) for the clock screen and settings menu, and can also write tzOffsetMinutes_/
  // use12h_/rtc_ via the Time & Date menu. Without this, encode()'s progressive parse into
  // TinyGPSPlus's internal structs races the format*() calls reading it, and RTC I2C
  // transactions from update()/setManualDateTime() could interleave. Recursive because
  // formatTime()/formatDate()/formatTimeParts()/dayOfWeek() all call the also-locking now()
  // internally.
  mutable SemaphoreHandle_t mutex_ = nullptr;

  // Assumes the caller already holds mutex_.
  DateTime utcFromGps() const;
  void onGpsSentence();
  void applyGpsProbe(uint8_t idx);
  void serviceGpsLink();
};
