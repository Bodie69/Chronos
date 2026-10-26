#include "time_manager.h"
#include <Wire.h>

bool TimeManager::begin() {
  mutex_ = xSemaphoreCreateRecursiveMutex();

  prefs_.begin(NVS_NAMESPACE, false);
  tzOffsetMinutes_ = prefs_.getShort("tz", DEFAULT_UTC_OFFSET_MINUTES);
  use12h_ = prefs_.getBool("fmt12h", DEFAULT_USE_12H_FORMAT);
  prefs_.end();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  rtcPresent_ = rtc_.begin();
  if (rtcPresent_ && rtc_.lostPower()) {
    // No battery / first boot: seed with compile time until GPS or manual sync arrives.
    // __DATE__/__TIME__ are the build machine's local time but the RTC holds UTC, so
    // shift by the configured offset (assumes the build machine is in the same zone).
    DateTime built(F(__DATE__), F(__TIME__));
    rtc_.adjust(DateTime(built.unixtime() - (int32_t)tzOffsetMinutes_ * 60));
  }

  applyGpsProbe(0);

  return rtcPresent_;
}

static const uint32_t kGpsProbeBauds[] = GPS_PROBE_BAUDS;
static const uint8_t kGpsProbeBaudCount = sizeof(kGpsProbeBauds) / sizeof(kGpsProbeBauds[0]);
static const uint8_t kGpsProbeCount = kGpsProbeBaudCount * 2; // each baud on normal, then swapped RX

// Assumes mutex_ is held (or begin() before tasks start).
void TimeManager::applyGpsProbe(uint8_t idx) {
  gpsProbeIdx_ = idx % kGpsProbeCount;
  bool swapped = gpsProbeIdx_ >= kGpsProbeBaudCount;
  gpsBaud_ = kGpsProbeBauds[gpsProbeIdx_ % kGpsProbeBaudCount];
  gpsSerial_.end(); // detaches the previous RX/TX so a swapped probe can't leave TX driving the RX pin
  if (swapped) {
    // GPS TX is landing on PIN_GPS_TX: listen there, RX-only (GPIO34 is input-only so it can't
    // become TX; the module needs no commands from us anyway).
    gpsRxPin_ = PIN_GPS_TX;
    gpsSerial_.begin(gpsBaud_, SERIAL_8N1, PIN_GPS_TX, -1);
  } else {
    gpsRxPin_ = PIN_GPS_RX;
    gpsSerial_.begin(gpsBaud_, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  }
  gpsProbeStartMs_ = millis();
  gpsProbeChecksumBase_ = gps_.passedChecksum();
}

// Assumes mutex_ is held.
void TimeManager::serviceGpsLink() {
  uint32_t passed = gps_.passedChecksum();
  if (passed != lastPassedChecksum_) {
    lastPassedChecksum_ = passed;
    lastValidSentenceMs_ = millis();
  }

  if (!gpsLinkFound_) {
    if (passed > gpsProbeChecksumBase_) {
      gpsLinkFound_ = true;
      Serial.printf("[GPS] Link OK: RX=GPIO%d @ %lu baud%s\n", gpsRxPin_, (unsigned long)gpsBaud_,
                    gpsRxPin_ == PIN_GPS_RX ? "" : " (TX/RX wires are swapped - works, but fix when convenient)");
    } else if (millis() - gpsProbeStartMs_ >= GPS_PROBE_DWELL_MS) {
      applyGpsProbe(gpsProbeIdx_ + 1);
    }
  } else if (millis() - lastValidSentenceMs_ >= GPS_LINK_LOST_MS) {
    gpsLinkFound_ = false;
    Serial.println("[GPS] Link lost, re-probing baud/pins");
    applyGpsProbe(gpsProbeIdx_); // retry current setting first
  }

  if (millis() - lastGpsLogMs_ >= 10000) {
    lastGpsLogMs_ = millis();
    Serial.printf("[GPS] %s RX=GPIO%d @ %lu  chars=%lu ok=%lu badcsum=%lu  sats=%lu fix=%s\n",
                  gpsLinkFound_ ? "link" : "probing", gpsRxPin_, (unsigned long)gpsBaud_,
                  (unsigned long)gps_.charsProcessed(), (unsigned long)passed,
                  (unsigned long)gps_.failedChecksum(),
                  (unsigned long)(gps_.satellites.isValid() ? gps_.satellites.value() : 0),
                  gpsLocked_ ? "YES" : "no");
  }
}

void TimeManager::setTimezoneOffsetMinutes(int16_t minutes) {
  // Not held across the flash (Preferences) write — that would block Core 0's time-critical
  // loop for however long the NVS write takes.
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  tzOffsetMinutes_ = minutes;
  xSemaphoreGiveRecursive(mutex_);
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putShort("tz", minutes);
  prefs_.end();
}

void TimeManager::setUse12HourFormat(bool v) {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  use12h_ = v;
  xSemaphoreGiveRecursive(mutex_);
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putBool("fmt12h", v);
  prefs_.end();
}

void TimeManager::update() {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  while (gpsSerial_.available()) {
    lastGpsByteMs_ = millis();
    gpsEverReceived_ = true;
    if (gps_.encode(gpsSerial_.read())) {
      onGpsSentence();
    }
  }

  // TinyGPSPlus's isValid() latches true forever, so lock is judged by how recently a fixed
  // RMC anchored the clock — otherwise a lost signal would freeze the time at the last fix.
  gpsLocked_ = gpsAnchorValid_ && (millis() - gpsAnchorMs_ < GPS_LOCK_TIMEOUT_MS);
  serviceGpsLink();
  xSemaphoreGiveRecursive(mutex_);
}

// Called after each complete, checksum-valid NMEA sentence. Assumes mutex_ is held.
void TimeManager::onGpsSentence() {
  // Only RMC commits date and time together (GGA commits time alone, which paired with the
  // previous RMC's date would be a day behind across the UTC midnight rollover). RMC commits
  // date/time even with status 'V' (no fix), but location only with 'A' — so location
  // updated in the same sentence means the receiver vouches for this time.
  bool isRmc = gps_.date.isUpdated() && gps_.time.isUpdated();
  bool hasFix = gps_.location.isUpdated();

  if (isRmc && hasFix && gps_.date.year() >= GPS_MIN_VALID_YEAR) {
    DateTime utc = utcFromGps();
    gpsAnchorUnix_ = utc.unixtime();
    gpsAnchorMs_ = millis();
    gpsAnchorValid_ = true;

    // Sync immediately on first lock (the RTC may hold a compile-time seed), then periodically.
    if (rtcPresent_ && (!rtcSyncedFromGps_ ||
                        millis() - lastRtcSyncFromGpsMs_ >= RTC_RESYNC_FROM_GPS_INTERVAL_MS)) {
      rtc_.adjust(utc);
      rtcSyncedFromGps_ = true;
      lastRtcSyncFromGpsMs_ = millis();
    }
  }

  // Reading clears the updated flags so the next sentence is judged on its own content.
  (void)gps_.date.value();
  (void)gps_.time.value();
  (void)gps_.location.rawLat();
}

bool TimeManager::isGpsLocked() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  bool v = gpsLocked_;
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

bool TimeManager::isGpsReceivingData() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  bool v = gpsEverReceived_ && (millis() - lastGpsByteMs_ < GPS_DATA_TIMEOUT_MS);
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

uint8_t TimeManager::satelliteCount() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  uint8_t v = gps_.satellites.age() < GPS_LOCK_TIMEOUT_MS ? gps_.satellites.value() : 0;
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

float TimeManager::gpsHdop() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  float v = gps_.hdop.age() < GPS_LOCK_TIMEOUT_MS ? gps_.hdop.hdop() : 99.9f;
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

int16_t TimeManager::timezoneOffsetMinutes() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  int16_t v = tzOffsetMinutes_;
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

bool TimeManager::use12HourFormat() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  bool v = use12h_;
  xSemaphoreGiveRecursive(mutex_);
  return v;
}

DateTime TimeManager::utcFromGps() const {
  return DateTime(gps_.date.year(), gps_.date.month(), gps_.date.day(),
                  gps_.time.hour(), gps_.time.minute(), gps_.time.second());
}

DateTime TimeManager::now() const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  DateTime utc;
  if (gpsLocked_) {
    utc = DateTime(gpsAnchorUnix_ + (uint32_t)((millis() - gpsAnchorMs_) / 1000));
  } else if (rtcPresent_) {
    utc = rtc_.now();
  } else {
    utc = DateTime((uint32_t)(millis() / 1000)); // degraded fallback, no real reference
  }
  DateTime local(utc.unixtime() + (int32_t)tzOffsetMinutes_ * 60);
  xSemaphoreGiveRecursive(mutex_);
  return local;
}

void TimeManager::setManualDateTime(const DateTime &localDt) {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  // Convert local -> UTC and store to RTC; GPS (if later locked) remains authoritative.
  DateTime utc(localDt.unixtime() - (int32_t)tzOffsetMinutes_ * 60);
  if (rtcPresent_) {
    rtc_.adjust(utc);
  }
  xSemaphoreGiveRecursive(mutex_);
}

uint8_t TimeManager::dayOfWeek() const {
  return now().dayOfTheWeek(); // RTClib: 0=Sunday
}

static const char *const kMonthAbbrev[] = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};
static const char *const kDayAbbrev[] = {
  "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

void TimeManager::formatTime(char *out, size_t outLen) const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  DateTime dt = now();
  if (use12h_) {
    uint8_t h12 = dt.hour() % 12;
    if (h12 == 0) h12 = 12;
    snprintf(out, outLen, "%02d:%02d:%02d %s", h12, dt.minute(), dt.second(),
             dt.hour() >= 12 ? "PM" : "AM");
  } else {
    snprintf(out, outLen, "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
  }
  xSemaphoreGiveRecursive(mutex_);
}

void TimeManager::formatTimeParts(char *hm, size_t hmLen, char *ampm, size_t ampmLen) const {
  xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
  DateTime dt = now();
  if (use12h_) {
    uint8_t h12 = dt.hour() % 12;
    if (h12 == 0) h12 = 12;
    snprintf(hm, hmLen, "%02d:%02d", h12, dt.minute());
    snprintf(ampm, ampmLen, "%s", dt.hour() >= 12 ? "PM" : "AM");
  } else {
    snprintf(hm, hmLen, "%02d:%02d", dt.hour(), dt.minute());
    if (ampmLen > 0) ampm[0] = '\0';
  }
  xSemaphoreGiveRecursive(mutex_);
}

void TimeManager::formatDate(char *out, size_t outLen) const {
  DateTime dt = now();
  uint8_t dow = dt.dayOfTheWeek();
  snprintf(out, outLen, "%s %s %d, %d",
           kDayAbbrev[dow], kMonthAbbrev[dt.month() - 1], dt.day(), dt.year());
}
