#include "alarm_manager.h"

// Alarms are stored under a key naming their target and in-target slot ("a<target>_<slot>"),
// not their flat array index, so changing MAX_ALARMS_PER_LAB never shifts saved alarms into
// another lab's list.
static void alarmKey(uint8_t slot, char *key, size_t keyLen) {
  snprintf(key, keyLen, "a%u_%u", slot / MAX_ALARMS_PER_LAB, slot % MAX_ALARMS_PER_LAB);
}

// Older firmware stored 10 slots per target under flat keys "al<target*10+slot>".
#define LEGACY_ALARMS_PER_LAB 10

void AlarmManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  for (auto &m : lastFiredMinuteToken_) m = UINT32_MAX;
  loadAll();
}

Alarm AlarmManager::getAlarm(uint8_t slot) const {
  if (slot >= TOTAL_ALARM_SLOTS) return Alarm();
  xSemaphoreTake(mutex_, portMAX_DELAY);
  Alarm a = alarms_[slot];
  xSemaphoreGive(mutex_);
  return a;
}

int AlarmManager::slotIndexForTarget(AlarmTarget target, uint8_t uiSlot) const {
  if (uiSlot < 1 || uiSlot > MAX_ALARMS_PER_LAB) return -1;
  return (int)target * MAX_ALARMS_PER_LAB + (uiSlot - 1);
}

void AlarmManager::saveAlarm(uint8_t slot, const Alarm &alarm) {
  if (slot >= TOTAL_ALARM_SLOTS) return;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  alarms_[slot] = alarm;
  xSemaphoreGive(mutex_);

  // Local Preferences (not prefs_) and the caller's copy (not alarms_[slot]): saveAlarm() is
  // called from both the UI task and the web dashboard task, which must not share one handle
  // or read the live array outside the lock.
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  char key[16];
  alarmKey(slot, key, sizeof(key));
  prefs.putBytes(key, &alarm, sizeof(Alarm));
  prefs.end();
}

void AlarmManager::saveAll() {
  prefs_.begin(NVS_NAMESPACE, false);
  for (uint8_t i = 0; i < TOTAL_ALARM_SLOTS; i++) {
    char key[16];
    alarmKey(i, key, sizeof(key));
    prefs_.putBytes(key, &alarms_[i], sizeof(Alarm));
  }
  prefs_.end();
}

void AlarmManager::loadAll() {
  // Opened read-write (not read-only) so a fresh board's nonexistent namespace is silently
  // created instead of logging an error on every first boot.
  prefs_.begin(NVS_NAMESPACE, false);
  for (uint8_t i = 0; i < TOTAL_ALARM_SLOTS; i++) {
    char key[16];
    alarmKey(i, key, sizeof(key));
    // isKey() check avoids calling getBytes() on a key that was never saved (every slot, on
    // a fresh board) — getBytes() logs a NOT_FOUND error internally for a missing key, which
    // is expected/harmless here but noisy across every slot every single boot.
    bool loaded = false;
    if (prefs_.isKey(key)) {
      Alarm a;
      if (prefs_.getBytes(key, &a, sizeof(Alarm)) == sizeof(Alarm)) {
        alarms_[i] = a;
        loaded = true;
      }
    }
    // One-time migration: move an alarm saved by older firmware to its new key, keeping it
    // in the same lab and slot number.
    uint8_t slotInTarget = i % MAX_ALARMS_PER_LAB;
    if (!loaded && slotInTarget < LEGACY_ALARMS_PER_LAB) {
      char legacyKey[16];
      snprintf(legacyKey, sizeof(legacyKey), "al%d",
               (i / MAX_ALARMS_PER_LAB) * LEGACY_ALARMS_PER_LAB + slotInTarget);
      if (prefs_.isKey(legacyKey)) {
        Alarm a;
        if (prefs_.getBytes(legacyKey, &a, sizeof(Alarm)) == sizeof(Alarm)) {
          alarms_[i] = a;
          loaded = true;
          prefs_.putBytes(key, &a, sizeof(Alarm));
          prefs_.remove(legacyKey);
        }
      }
    }
    if (!loaded) {
      // Leave struct default; assign a sensible default target so slots are distinguishable.
      alarms_[i].target = (AlarmTarget)(i / MAX_ALARMS_PER_LAB);
      alarms_[i].enabled = false;
    }
  }
  prefs_.end();
}

bool AlarmManager::checkDue(uint8_t hour, uint8_t minute, uint8_t dayMask, uint32_t nowMinuteToken,
                             AlarmTrigger &outTrigger) {
  bool any = false;
  outTrigger.fireLab1 = false;
  outTrigger.fireLab2 = false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  for (uint8_t i = 0; i < TOTAL_ALARM_SLOTS; i++) {
    Alarm &a = alarms_[i];
    if (!a.enabled) continue;
    if (a.hour != hour || a.minute != minute) continue;
    if ((a.dayMask & dayMask) == 0) continue;
    if (lastFiredMinuteToken_[i] == nowMinuteToken) continue; // already fired this exact minute

    lastFiredMinuteToken_[i] = nowMinuteToken;
    any = true;
    if (a.target == TARGET_LAB1) {
      outTrigger.fireLab1 = true;
      outTrigger.alarmLab1 = a; // copy while locked; see AlarmTrigger comment
    }
    if (a.target == TARGET_LAB2) {
      outTrigger.fireLab2 = true;
      outTrigger.alarmLab2 = a; // copy while locked; see AlarmTrigger comment
    }
  }
  xSemaphoreGive(mutex_);
  return any;
}

bool AlarmManager::findNextAlarm(uint8_t forLabIndex, uint8_t curHour, uint8_t curMinute,
                                  uint8_t curDayMask, Alarm &outAlarm, uint16_t &outMinutesUntil) const {
  int16_t curMinuteOfDay = (int16_t)curHour * 60 + curMinute;
  int bestDelta = INT16_MAX;
  bool found = false;
  AlarmTarget wantedA = (forLabIndex == 0) ? TARGET_LAB1 : TARGET_LAB2;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  for (uint8_t i = 0; i < TOTAL_ALARM_SLOTS; i++) {
    const Alarm &a = alarms_[i];
    if (!a.enabled) continue;
    if (a.target != wantedA) continue;

    // Search over the next 7 days for the nearest occurrence. curDayMask carries only
    // today's single day-of-week bit; reconstruct that index to project future days.
    uint8_t todayIndex = 0;
    for (uint8_t b = 0; b < 7; b++) {
      if (curDayMask & (1 << b)) { todayIndex = b; break; }
    }

    for (uint8_t dayOffset = 0; dayOffset < 7; dayOffset++) {
      uint8_t candidateDow = (todayIndex + dayOffset) % 7;
      if ((a.dayMask & (1 << candidateDow)) == 0) continue;

      int16_t alarmMinuteOfDay = (int16_t)a.hour * 60 + a.minute;
      int32_t delta;
      if (dayOffset == 0) {
        if (alarmMinuteOfDay <= curMinuteOfDay) continue; // today's slot already passed
        delta = alarmMinuteOfDay - curMinuteOfDay;
      } else {
        delta = (int32_t)dayOffset * 24 * 60 + alarmMinuteOfDay - curMinuteOfDay;
      }
      if (delta < bestDelta) {
        bestDelta = delta;
        outAlarm = a;
        found = true;
      }
      break; // only nearest occurrence per alarm needed per day scan iteration
    }
  }
  xSemaphoreGive(mutex_);

  if (found) outMinutesUntil = (uint16_t)bestDelta;
  return found;
}
