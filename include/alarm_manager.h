#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

#define TOTAL_ALARM_SLOTS (MAX_ALARMS_PER_LAB * 2) // MAX_ALARMS_PER_LAB each for Lab1 / Lab2

struct Alarm {
  bool enabled = false;
  AlarmTarget target = TARGET_LAB1;
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint8_t dayMask = DAYMASK_WEEKDAYS;
  uint16_t durationSec = 10; // 0 = ring until dismissed
  ChimeStyle chime = CHIME_STANDARD_PULSE;
  char label[ALARM_LABEL_MAX_LEN] = "CLASS START";
};

// Result of an alarm scan for the current minute tick. Lab1 and Lab2 get their own Alarm
// copy (rather than one shared pointer, and rather than a pointer into AlarmManager's
// internal array) since a Lab1-targeted and a Lab2-targeted alarm can be due in the
// same minute with different chimes/durations/labels, and a pointer into the live array
// could be mutated by AlarmManager::saveAlarm() (called from the UI core) the instant after
// checkDue() releases its internal lock.
struct AlarmTrigger {
  bool fireLab1 = false;
  bool fireLab2 = false;
  Alarm alarmLab1;
  Alarm alarmLab2;
};

class AlarmManager {
public:
  void begin();

  // Total addressable slots (MAX_ALARMS_PER_LAB per target category).
  static constexpr uint8_t totalSlots() { return TOTAL_ALARM_SLOTS; }

  // Slot index within a target's own 1-MAX_ALARMS_PER_LAB numbering, as shown in the UI.
  int slotIndexForTarget(AlarmTarget target, uint8_t uiSlot) const;

  // Thread-safe: returns a copy under the internal lock rather than a reference into
  // alarms_[], so the caller (typically the Core-1 UI menu) can't observe a torn struct
  // while Core 0's checkDue()/findNextAlarm() or a concurrent saveAlarm() is mid-write.
  Alarm getAlarm(uint8_t slot) const;

  void saveAlarm(uint8_t slot, const Alarm &alarm);
  void saveAll();
  void loadAll();

  // Call once per minute-boundary from the scheduler task. `nowMinuteToken` must be an
  // absolute, non-wrapping minute counter (e.g. unixtime()/60) — using minute-of-day alone
  // for the "already fired" guard would make every alarm fire exactly once, ever, since
  // minute-of-day repeats every 24h. Returns true if any alarm matched.
  bool checkDue(uint8_t hour, uint8_t minute, uint8_t dayMask, uint32_t nowMinuteToken,
                AlarmTrigger &outTrigger);

  // Finds the soonest upcoming enabled alarm for a given lab (TARGET_LAB1/TARGET_LAB2).
  // Not currently drawn on any screen (the clock
  // screen's 3-row header/time/footer layout has no room left for a "next alarm" line) —
  // kept as a ready-to-use query if a future layout adds one back.
  bool findNextAlarm(uint8_t forLabIndex /*0 or 1*/, uint8_t curHour, uint8_t curMinute,
                      uint8_t curDayMask, Alarm &outAlarm, uint16_t &outMinutesUntil) const;

private:
  Alarm alarms_[TOTAL_ALARM_SLOTS];
  Preferences prefs_;

  // Absolute epoch-minute each slot last fired at (UINT32_MAX = never fired). Must be
  // absolute rather than minute-of-day so a daily alarm fires again on the next day.
  uint32_t lastFiredMinuteToken_[TOTAL_ALARM_SLOTS];

  // Guards alarms_[] against concurrent access: Core 0's timeAlarmTask calls checkDue()
  // every loop tick while Core 1's UI menu calls saveAlarm()/getAlarm() when the user edits
  // an alarm. Not held across flash (Preferences) I/O in saveAlarm() — that would block
  // Core 0's time-critical loop for however long the NVS write takes.
  mutable SemaphoreHandle_t mutex_ = nullptr;
};
