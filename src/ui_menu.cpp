#include "ui_menu.h"

// Comlab 1 and Comlab 2 alarms each get their own top-level entry (rather than a shared
// "Manage Alarms" -> target-picker step) so the two labs are clearly separate, independently
// reachable settings — matching how their alarms already ring independently.
// Items 0/1 are placeholders: renderMainMenu() replaces them with "<lab name> Alarms" using
// the names set from the web dashboard.
static const char *const kMainMenuItems[] = {
  "Comlab 1 Alarms", "Comlab 2 Alarms", "Time & Date", "Diagnostics", "Back to Clock"
};
#define MAIN_MENU_COUNT 5

static const char *const kTimeMenuItems[] = {
  "Timezone Offset", "12h / 24h Format", "Manual Set Time", "Back"
};
#define TIME_MENU_COUNT 4

static const char *const kDiagMenuItems[] = {
  "Test Speaker 1", "Test Speaker 2", "GPS Signal Monitor", "Brightness", "Volume", "Back"
};
#define DIAG_MENU_COUNT 6

static const char *const kLabelPresets[] = {
  "CLASS START", "CLASS END", "RECESS", "LUNCH", "DISMISSAL",
  "PERIOD 1", "PERIOD 2", "PERIOD 3", "PERIOD 4", "PERIOD 5",
  "PERIOD 6", "PERIOD 7", "PERIOD 8"
};
#define LABEL_PRESET_COUNT 13

static const char *const kChimeNames[] = {
  "Westminster", "High-Freq Bell", "Standard Pulse", "Double Chime"
};

static const uint16_t kDurationOptions[] = ALARM_DURATION_OPTIONS_SEC;

void UiMenu::begin(AlarmManager *alarms, TimeManager *time, BuzzerManager *buzzers,
                    DisplayManager *display, LabNames *labNames) {
  alarms_ = alarms;
  time_ = time;
  buzzers_ = buzzers;
  display_ = display;
  labNames_ = labNames;
}

void UiMenu::handleEvent(ButtonEvent evt, bool isRepeat) {
  if (evt == BTN_EVENT_NONE) return;

  if (isRepeat) {
    switch (screen_) {
      case SCR_CLOCK:
      case SCR_MAIN_MENU:
      case SCR_ALARM_SLOT:
      case SCR_ALARM_EDIT_DAYS:
      case SCR_ALARM_EDIT_LABEL:
      case SCR_TIME_MENU:
      case SCR_TIME_FORMAT:
      case SCR_DIAG_MENU:
      case SCR_DIAG_GPS:
        return; // menus/toggles: one step per press, never auto-scroll
      default: break;
    }
  }

  switch (screen_) {
    case SCR_CLOCK:
      if (evt == BTN_EVENT_SELECT) goTo(SCR_MAIN_MENU);
      break;
    case SCR_MAIN_MENU: handleMainMenu(evt); break;
    case SCR_ALARM_SLOT: handleAlarmSlot(evt); break;
    case SCR_ALARM_EDIT: handleAlarmEdit(evt); break;
    case SCR_ALARM_EDIT_DAYS: handleAlarmEditDays(evt); break;
    case SCR_ALARM_EDIT_LABEL: handleAlarmEditLabel(evt); break;
    case SCR_TIME_MENU: handleTimeMenu(evt); break;
    case SCR_TIME_TZ: handleTimeTz(evt); break;
    case SCR_TIME_FORMAT: handleTimeFormat(evt); break;
    case SCR_TIME_MANUAL: handleTimeManual(evt); break;
    case SCR_DIAG_MENU: handleDiagMenu(evt); break;
    case SCR_DIAG_GPS: handleDiagGps(evt); break;
    case SCR_DIAG_BRIGHTNESS: handleDiagBrightness(evt); break;
    case SCR_DIAG_VOLUME: handleDiagVolume(evt); break;
  }
}

void UiMenu::render() {
  switch (screen_) {
    case SCR_CLOCK: return; // handled by main loop's clock drawing path
    case SCR_MAIN_MENU: renderMainMenu(); break;
    case SCR_ALARM_SLOT: renderAlarmSlot(); break;
    case SCR_ALARM_EDIT: renderAlarmEdit(); break;
    case SCR_ALARM_EDIT_DAYS: renderAlarmEditDays(); break;
    case SCR_ALARM_EDIT_LABEL: renderAlarmEditLabel(); break;
    case SCR_TIME_MENU: renderTimeMenu(); break;
    case SCR_TIME_TZ: renderTimeTz(); break;
    case SCR_TIME_FORMAT: renderTimeFormat(); break;
    case SCR_TIME_MANUAL: renderTimeManual(); break;
    case SCR_DIAG_MENU: renderDiagMenu(); break;
    case SCR_DIAG_GPS: renderDiagGps(); break;
    case SCR_DIAG_BRIGHTNESS: renderDiagBrightness(); break;
    case SCR_DIAG_VOLUME: renderDiagVolume(); break;
  }
}

// ---------------------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------------------
void UiMenu::renderMainMenu() {
  char labLines[2][LAB_NAME_MAX_LEN + 8];
  const char *items[MAIN_MENU_COUNT];
  for (uint8_t i = 0; i < MAIN_MENU_COUNT; i++) items[i] = kMainMenuItems[i];
  for (uint8_t lab = 0; lab < 2; lab++) {
    char name[LAB_NAME_MAX_LEN + 1];
    labNames_->get(lab, name, sizeof(name));
    snprintf(labLines[lab], sizeof(labLines[lab]), "%s Alarms", name);
    items[lab] = labLines[lab];
  }
  display_->drawMenuLines(0, "MAIN MENU", items, MAIN_MENU_COUNT, cursor_);
  display_->drawMenuLines(1, "MAIN MENU", items, MAIN_MENU_COUNT, cursor_);
}

void UiMenu::handleMainMenu(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: cursor_ = (cursor_ + MAIN_MENU_COUNT - 1) % MAIN_MENU_COUNT; break;
    case BTN_EVENT_DOWN: cursor_ = (cursor_ + 1) % MAIN_MENU_COUNT; break;
    case BTN_EVENT_BACK: goTo(SCR_CLOCK); break;
    case BTN_EVENT_SELECT:
      switch (cursor_) {
        case 0: editTarget_ = TARGET_LAB1; goTo(SCR_ALARM_SLOT); break;
        case 1: editTarget_ = TARGET_LAB2; goTo(SCR_ALARM_SLOT); break;
        case 2: goTo(SCR_TIME_MENU); break;
        case 3: goTo(SCR_DIAG_MENU); break;
        case 4: goTo(SCR_CLOCK); break;
      }
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------
// Alarm slot -> edit wizard (target is picked directly from the main menu, see
// UiMenu::handleMainMenu, so each lab's alarms are reached independently)
// ---------------------------------------------------------------------------------------
static void slotScreenTitle(LabNames *labNames, AlarmTarget target, char *out, size_t outLen) {
  char name[LAB_NAME_MAX_LEN + 1];
  labNames->get(target == TARGET_LAB1 ? 0 : 1, name, sizeof(name));
  snprintf(out, outLen, "%s SLOT", name);
}

void UiMenu::renderAlarmSlot() {
  char lines[MAX_ALARMS_PER_LAB][24];
  const char *ptrs[MAX_ALARMS_PER_LAB];
  for (uint8_t i = 0; i < MAX_ALARMS_PER_LAB; i++) {
    int idx = alarms_->slotIndexForTarget(editTarget_, i + 1);
    Alarm a = alarms_->getAlarm(idx);
    if (a.enabled) {
      snprintf(lines[i], sizeof(lines[i]), "%2d %02d:%02d %s", i + 1, a.hour, a.minute, a.label);
    } else {
      snprintf(lines[i], sizeof(lines[i]), "%2d --:-- (off)", i + 1);
    }
    ptrs[i] = lines[i];
  }
  char title[LAB_NAME_MAX_LEN + 8];
  slotScreenTitle(labNames_, editTarget_, title, sizeof(title));
  display_->drawMenuLines(0, title, ptrs, MAX_ALARMS_PER_LAB, cursor_);
  display_->drawMenuLines(1, title, ptrs, MAX_ALARMS_PER_LAB, cursor_);
}

void UiMenu::handleAlarmSlot(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: cursor_ = (cursor_ + MAX_ALARMS_PER_LAB - 1) % MAX_ALARMS_PER_LAB; break;
    case BTN_EVENT_DOWN: cursor_ = (cursor_ + 1) % MAX_ALARMS_PER_LAB; break;
    case BTN_EVENT_BACK: goTo(SCR_MAIN_MENU); break;
    case BTN_EVENT_SELECT: {
      editSlotUi_ = cursor_ + 1;
      int idx = alarms_->slotIndexForTarget(editTarget_, editSlotUi_);
      editBuffer_ = alarms_->getAlarm(idx);
      editBuffer_.target = editTarget_;
      editField_ = FIELD_STATUS;
      // Find preset label index (defaults to 0 if custom/unmatched).
      editLabelIndex_ = 0;
      for (uint8_t i = 0; i < LABEL_PRESET_COUNT; i++) {
        if (strncmp(editBuffer_.label, kLabelPresets[i], ALARM_LABEL_MAX_LEN) == 0) {
          editLabelIndex_ = i;
          break;
        }
      }
      goTo(SCR_ALARM_EDIT);
      break;
    }
    default: break;
  }
}

void UiMenu::renderAlarmEdit() {
  char lines[7][24];
  const char *ptrs[7];
  snprintf(lines[0], sizeof(lines[0]), "Status: %s", editBuffer_.enabled ? "ON" : "OFF");
  snprintf(lines[1], sizeof(lines[1]), "Hour:   %02d", editBuffer_.hour);
  snprintf(lines[2], sizeof(lines[2]), "Minute: %02d", editBuffer_.minute);
  const char *daysDesc = (editBuffer_.dayMask == DAYMASK_DAILY) ? "Daily"
                        : (editBuffer_.dayMask == DAYMASK_WEEKDAYS) ? "Weekdays"
                        : "Custom";
  snprintf(lines[3], sizeof(lines[3]), "Days:   %s", daysDesc);
  if (editBuffer_.durationSec == 0) {
    snprintf(lines[4], sizeof(lines[4]), "Dur:    Until stop");
  } else {
    snprintf(lines[4], sizeof(lines[4]), "Dur:    %us", editBuffer_.durationSec);
  }
  snprintf(lines[5], sizeof(lines[5]), "Chime:  %s", kChimeNames[editBuffer_.chime]);
  snprintf(lines[6], sizeof(lines[6]), "Label:  %s", editBuffer_.label);
  for (uint8_t i = 0; i < 7; i++) ptrs[i] = lines[i];

  char title[20];
  snprintf(title, sizeof(title), "EDIT ALARM %u", editSlotUi_);
  display_->drawMenuLines(0, title, ptrs, 7, editField_ <= FIELD_LABEL ? editField_ : -1);
  display_->drawMenuLines(1, title, ptrs, 7, editField_ <= FIELD_LABEL ? editField_ : -1);
}

void UiMenu::handleAlarmEdit(ButtonEvent evt) {
  if (evt == BTN_EVENT_BACK) {
    goTo(SCR_ALARM_SLOT);
    return;
  }

  switch (editField_) {
    case FIELD_STATUS:
      if (evt == BTN_EVENT_UP || evt == BTN_EVENT_DOWN) editBuffer_.enabled = !editBuffer_.enabled;
      break;
    case FIELD_HOUR:
      if (evt == BTN_EVENT_UP) editBuffer_.hour = (editBuffer_.hour + 1) % 24;
      if (evt == BTN_EVENT_DOWN) editBuffer_.hour = (editBuffer_.hour + 23) % 24;
      break;
    case FIELD_MINUTE:
      if (evt == BTN_EVENT_UP) editBuffer_.minute = (editBuffer_.minute + 1) % 60;
      if (evt == BTN_EVENT_DOWN) editBuffer_.minute = (editBuffer_.minute + 59) % 60;
      break;
    case FIELD_DAYS:
      if (evt == BTN_EVENT_SELECT) {
        editDayCursor_ = 0;
        goTo(SCR_ALARM_EDIT_DAYS);
        return;
      }
      if (evt == BTN_EVENT_UP || evt == BTN_EVENT_DOWN) {
        // Cycle quick presets; use SELECT to enter full custom toggle screen instead.
        editBuffer_.dayMask = (editBuffer_.dayMask == DAYMASK_WEEKDAYS) ? DAYMASK_DAILY
                              : DAYMASK_WEEKDAYS;
      }
      break;
    case FIELD_DURATION:
      if (evt == BTN_EVENT_UP || evt == BTN_EVENT_DOWN) {
        int8_t curIdx = 0;
        for (uint8_t i = 0; i < ALARM_DURATION_OPTIONS_COUNT; i++) {
          if (kDurationOptions[i] == editBuffer_.durationSec) { curIdx = i; break; }
        }
        curIdx = (evt == BTN_EVENT_UP)
          ? (curIdx + 1) % ALARM_DURATION_OPTIONS_COUNT
          : (curIdx + ALARM_DURATION_OPTIONS_COUNT - 1) % ALARM_DURATION_OPTIONS_COUNT;
        editBuffer_.durationSec = kDurationOptions[curIdx];
      }
      break;
    case FIELD_CHIME:
      if (evt == BTN_EVENT_UP) editBuffer_.chime = (ChimeStyle)((editBuffer_.chime + 1) % CHIME_STYLE_COUNT);
      if (evt == BTN_EVENT_DOWN) editBuffer_.chime = (ChimeStyle)((editBuffer_.chime + CHIME_STYLE_COUNT - 1) % CHIME_STYLE_COUNT);
      break;
    case FIELD_LABEL:
      if (evt == BTN_EVENT_SELECT) {
        goTo(SCR_ALARM_EDIT_LABEL);
        return;
      }
      break;
    default: break;
  }

  if (evt == BTN_EVENT_SELECT && editField_ != FIELD_DAYS && editField_ != FIELD_LABEL) {
    editField_ = (AlarmEditField)(editField_ + 1);
    if (editField_ == FIELD_DONE) {
      int idx = alarms_->slotIndexForTarget(editTarget_, editSlotUi_);
      alarms_->saveAlarm(idx, editBuffer_);
      goTo(SCR_ALARM_SLOT);
    }
  }
}

void UiMenu::renderAlarmEditDays() {
  static const char *dayLetters[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
  char lines[7][12];
  const char *ptrs[7];
  for (uint8_t i = 0; i < 7; i++) {
    bool on = editBuffer_.dayMask & (1 << i);
    snprintf(lines[i], sizeof(lines[i]), "[%s] %s", on ? "X" : " ", dayLetters[i]);
    ptrs[i] = lines[i];
  }
  display_->drawMenuLines(0, "TOGGLE DAYS", ptrs, 7, editDayCursor_);
  display_->drawMenuLines(1, "TOGGLE DAYS", ptrs, 7, editDayCursor_);
}

void UiMenu::handleAlarmEditDays(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: editDayCursor_ = (editDayCursor_ + 6) % 7; break;
    case BTN_EVENT_DOWN: editDayCursor_ = (editDayCursor_ + 1) % 7; break;
    case BTN_EVENT_SELECT: editBuffer_.dayMask ^= (1 << editDayCursor_); break;
    case BTN_EVENT_BACK: goTo(SCR_ALARM_EDIT); editField_ = (AlarmEditField)(FIELD_DAYS + 1); break;
    default: break;
  }
}

void UiMenu::renderAlarmEditLabel() {
  const char *ptrs[LABEL_PRESET_COUNT];
  for (uint8_t i = 0; i < LABEL_PRESET_COUNT; i++) ptrs[i] = kLabelPresets[i];
  display_->drawMenuLines(0, "SELECT LABEL", ptrs, LABEL_PRESET_COUNT, editLabelIndex_);
  display_->drawMenuLines(1, "SELECT LABEL", ptrs, LABEL_PRESET_COUNT, editLabelIndex_);
}

void UiMenu::handleAlarmEditLabel(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: editLabelIndex_ = (editLabelIndex_ + LABEL_PRESET_COUNT - 1) % LABEL_PRESET_COUNT; break;
    case BTN_EVENT_DOWN: editLabelIndex_ = (editLabelIndex_ + 1) % LABEL_PRESET_COUNT; break;
    case BTN_EVENT_SELECT: {
      strncpy(editBuffer_.label, kLabelPresets[editLabelIndex_], ALARM_LABEL_MAX_LEN - 1);
      editBuffer_.label[ALARM_LABEL_MAX_LEN - 1] = '\0';
      editField_ = (AlarmEditField)(FIELD_LABEL + 1);
      int idx = alarms_->slotIndexForTarget(editTarget_, editSlotUi_);
      alarms_->saveAlarm(idx, editBuffer_);
      goTo(SCR_ALARM_SLOT);
      break;
    }
    case BTN_EVENT_BACK:
      editField_ = (AlarmEditField)(FIELD_LABEL + 1);
      goTo(SCR_ALARM_EDIT);
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------
// Time & Date menu
// ---------------------------------------------------------------------------------------
void UiMenu::renderTimeMenu() {
  display_->drawMenuLines(0, "TIME & DATE", kTimeMenuItems, TIME_MENU_COUNT, cursor_);
  display_->drawMenuLines(1, "TIME & DATE", kTimeMenuItems, TIME_MENU_COUNT, cursor_);
}

void UiMenu::handleTimeMenu(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: cursor_ = (cursor_ + TIME_MENU_COUNT - 1) % TIME_MENU_COUNT; break;
    case BTN_EVENT_DOWN: cursor_ = (cursor_ + 1) % TIME_MENU_COUNT; break;
    case BTN_EVENT_BACK: goTo(SCR_MAIN_MENU); break;
    case BTN_EVENT_SELECT:
      switch (cursor_) {
        case 0: goTo(SCR_TIME_TZ); break;
        case 1: goTo(SCR_TIME_FORMAT); break;
        case 2:
          manualEdit_ = time_->now();
          manualField_ = 0;
          goTo(SCR_TIME_MANUAL);
          break;
        case 3: goTo(SCR_MAIN_MENU); break;
      }
      break;
    default: break;
  }
}

void UiMenu::renderTimeTz() {
  char line[24];
  int16_t off = time_->timezoneOffsetMinutes();
  snprintf(line, sizeof(line), "UTC%+03d:%02d", off / 60, abs(off % 60));
  const char *ptrs[1] = { line };
  display_->drawMenuLines(0, "TIMEZONE (SEL=OK)", ptrs, 1, 0);
  display_->drawMenuLines(1, "TIMEZONE (SEL=OK)", ptrs, 1, 0);
}

void UiMenu::handleTimeTz(ButtonEvent evt) {
  int16_t off = time_->timezoneOffsetMinutes();
  switch (evt) {
    case BTN_EVENT_UP: time_->setTimezoneOffsetMinutes(off + 30); break;
    case BTN_EVENT_DOWN: time_->setTimezoneOffsetMinutes(off - 30); break;
    case BTN_EVENT_SELECT:
    case BTN_EVENT_BACK: goTo(SCR_TIME_MENU); break;
    default: break;
  }
}

void UiMenu::renderTimeFormat() {
  const char *line = time_->use12HourFormat() ? "12-Hour Format" : "24-Hour Format";
  const char *ptrs[1] = { line };
  display_->drawMenuLines(0, "TIME FORMAT", ptrs, 1, 0);
  display_->drawMenuLines(1, "TIME FORMAT", ptrs, 1, 0);
}

void UiMenu::handleTimeFormat(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP:
    case BTN_EVENT_DOWN:
      time_->setUse12HourFormat(!time_->use12HourFormat());
      break;
    case BTN_EVENT_SELECT:
    case BTN_EVENT_BACK: goTo(SCR_TIME_MENU); break;
    default: break;
  }
}

void UiMenu::renderTimeManual() {
  char lines[5][20];
  const char *ptrs[5];
  snprintf(lines[0], sizeof(lines[0]), "Year:   %d", manualEdit_.year());
  snprintf(lines[1], sizeof(lines[1]), "Month:  %02d", manualEdit_.month());
  snprintf(lines[2], sizeof(lines[2]), "Day:    %02d", manualEdit_.day());
  snprintf(lines[3], sizeof(lines[3]), "Hour:   %02d", manualEdit_.hour());
  snprintf(lines[4], sizeof(lines[4]), "Minute: %02d", manualEdit_.minute());
  for (uint8_t i = 0; i < 5; i++) ptrs[i] = lines[i];
  display_->drawMenuLines(0, "MANUAL SET TIME", ptrs, 5, manualField_);
  display_->drawMenuLines(1, "MANUAL SET TIME", ptrs, 5, manualField_);
}

static uint8_t daysInMonth(int year, int month) {
  static const uint8_t kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (month == 2 && leap) return 29;
  return kDays[month - 1];
}

void UiMenu::handleTimeManual(ButtonEvent evt) {
  int y = manualEdit_.year(), mo = manualEdit_.month(), d = manualEdit_.day();
  int h = manualEdit_.hour(), mi = manualEdit_.minute();

  if (evt == BTN_EVENT_BACK) { goTo(SCR_TIME_MENU); return; }

  if (evt == BTN_EVENT_UP || evt == BTN_EVENT_DOWN) {
    int delta = (evt == BTN_EVENT_UP) ? 1 : -1;
    switch (manualField_) {
      case 0: y += delta; break;
      case 1: mo = ((mo - 1 + delta + 12) % 12) + 1; break;
      case 3: h = (h + delta + 24) % 24; break;
      case 4: mi = (mi + delta + 60) % 60; break;
      case 2: {
        uint8_t maxDay = daysInMonth(y, mo);
        d = ((d - 1 + delta + maxDay) % maxDay) + 1;
        break;
      }
    }
    // Changing year/month can leave `d` past the new month's length (e.g. Mar 31 -> Feb).
    uint8_t maxDayForMonth = daysInMonth(y, mo);
    if (d > maxDayForMonth) d = maxDayForMonth;
    manualEdit_ = DateTime(y, mo, d, h, mi, 0);
  } else if (evt == BTN_EVENT_SELECT) {
    if (manualField_ < 4) {
      manualField_++;
    } else {
      time_->setManualDateTime(manualEdit_);
      goTo(SCR_TIME_MENU);
    }
  }
}

// ---------------------------------------------------------------------------------------
// Diagnostics menu
// ---------------------------------------------------------------------------------------
void UiMenu::renderDiagMenu() {
  display_->drawMenuLines(0, "DIAGNOSTICS", kDiagMenuItems, DIAG_MENU_COUNT, cursor_);
  display_->drawMenuLines(1, "DIAGNOSTICS", kDiagMenuItems, DIAG_MENU_COUNT, cursor_);
}

void UiMenu::handleDiagMenu(ButtonEvent evt) {
  switch (evt) {
    case BTN_EVENT_UP: cursor_ = (cursor_ + DIAG_MENU_COUNT - 1) % DIAG_MENU_COUNT; break;
    case BTN_EVENT_DOWN: cursor_ = (cursor_ + 1) % DIAG_MENU_COUNT; break;
    case BTN_EVENT_BACK: goTo(SCR_MAIN_MENU); break;
    case BTN_EVENT_SELECT:
      switch (cursor_) {
        case 0: buzzers_->requestTestBeep(0); break;
        case 1: buzzers_->requestTestBeep(1); break;
        case 2: goTo(SCR_DIAG_GPS); break;
        case 3: goTo(SCR_DIAG_BRIGHTNESS); break;
        case 4: goTo(SCR_DIAG_VOLUME); break;
        case 5: goTo(SCR_MAIN_MENU); break;
      }
      break;
    default: break;
  }
}

void UiMenu::renderDiagGps() {
  char lines[3][24];
  const char *ptrs[3];
  snprintf(lines[0], sizeof(lines[0]), "Fix: %s",
           time_->isGpsLocked() ? "LOCKED" : (time_->isGpsReceivingData() ? "NO FIX" : "NO DATA"));
  snprintf(lines[1], sizeof(lines[1]), "Sats: %u", time_->satelliteCount());
  float hdop = time_->gpsHdop();
  if (hdop >= 99.9f) {
    snprintf(lines[2], sizeof(lines[2]), "Signal: --");
  } else {
    snprintf(lines[2], sizeof(lines[2]), "Signal: %.1f HDOP", hdop);
  }
  ptrs[0] = lines[0]; ptrs[1] = lines[1]; ptrs[2] = lines[2];
  display_->drawMenuLines(0, "GPS MONITOR (BACK)", ptrs, 3, -1);
  display_->drawMenuLines(1, "GPS MONITOR (BACK)", ptrs, 3, -1);
}

void UiMenu::handleDiagGps(ButtonEvent evt) {
  if (evt == BTN_EVENT_BACK || evt == BTN_EVENT_SELECT) goTo(SCR_DIAG_MENU);
}

void UiMenu::renderDiagBrightness() {
  char line[24];
  snprintf(line, sizeof(line), "Brightness: %u", display_->getBrightness());
  const char *ptrs[1] = { line };
  display_->drawMenuLines(0, "BRIGHTNESS", ptrs, 1, 0);
  display_->drawMenuLines(1, "BRIGHTNESS", ptrs, 1, 0);
}

void UiMenu::handleDiagBrightness(ButtonEvent evt) {
  uint8_t b = display_->getBrightness();
  switch (evt) {
    case BTN_EVENT_UP:
      display_->setBrightness((b >= 250) ? 255 : b + 5);
      break;
    case BTN_EVENT_DOWN:
      display_->setBrightness((b <= 6) ? 1 : b - 5);
      break;
    case BTN_EVENT_SELECT:
    case BTN_EVENT_BACK:
      goTo(SCR_DIAG_MENU);
      break;
    default: break;
  }
}

void UiMenu::renderDiagVolume() {
  char line[24];
  snprintf(line, sizeof(line), "Volume: %u%%", buzzers_->volume());
  const char *ptrs[1] = { line };
  display_->drawMenuLines(0, "VOLUME (SEL=TEST)", ptrs, 1, 0);
  display_->drawMenuLines(1, "VOLUME (SEL=TEST)", ptrs, 1, 0);
}

void UiMenu::handleDiagVolume(ButtonEvent evt) {
  uint8_t v = buzzers_->volume();
  switch (evt) {
    case BTN_EVENT_UP: buzzers_->setVolume(v >= 95 ? 100 : v + 5); break;
    case BTN_EVENT_DOWN: buzzers_->setVolume(v <= 5 ? 0 : v - 5); break;
    // SELECT plays a test tone on Comlab 1's speaker at the current volume.
    case BTN_EVENT_SELECT: buzzers_->requestTestBeep(0); break;
    case BTN_EVENT_BACK: goTo(SCR_DIAG_MENU); break;
    default: break;
  }
}
