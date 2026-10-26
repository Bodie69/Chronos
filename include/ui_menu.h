#pragma once

#include <Arduino.h>
#include "config.h"
#include "alarm_manager.h"
#include "time_manager.h"
#include "buzzer_manager.h"
#include "display_manager.h"
#include "button_manager.h"
#include "lab_names.h"

enum UiScreen : uint8_t {
  SCR_CLOCK = 0,
  SCR_MAIN_MENU,
  SCR_ALARM_SLOT,
  SCR_ALARM_EDIT,
  SCR_ALARM_EDIT_DAYS,
  SCR_ALARM_EDIT_LABEL,
  SCR_TIME_MENU,
  SCR_TIME_TZ,
  SCR_TIME_FORMAT,
  SCR_TIME_MANUAL,
  SCR_DIAG_MENU,
  SCR_DIAG_GPS,
  SCR_DIAG_BRIGHTNESS,
  SCR_DIAG_VOLUME
};

enum AlarmEditField : uint8_t {
  FIELD_STATUS = 0,
  FIELD_HOUR,
  FIELD_MINUTE,
  FIELD_DAYS,
  FIELD_DURATION,
  FIELD_CHIME,
  FIELD_LABEL,
  FIELD_DONE
};

// Top-level interactive UI state machine: handles button input, menu navigation, alarm
// editing wizard, time/date settings and diagnostics — then renders via DisplayManager and
// mirrors the menu across both lab viewports (both labs share the one physical keypad).
class UiMenu {
public:
  void begin(AlarmManager *alarms, TimeManager *time, BuzzerManager *buzzers,
             DisplayManager *display, LabNames *labNames);

  // Call every UI tick (~30-60Hz is plenty) with the latest button event. `isRepeat` marks
  // held-button auto-repeat; list menus ignore it so they only move on deliberate presses,
  // while value editors (hour/minute, brightness, timezone...) still fast-scroll when held.
  void handleEvent(ButtonEvent evt, bool isRepeat = false);

  // Renders the current screen. When in SCR_CLOCK, the caller (main loop) is expected to
  // instead call the clock-drawing path directly; render() only draws menu/wizard screens.
  void render();

  bool isOnClockScreen() const { return screen_ == SCR_CLOCK; }

private:
  AlarmManager *alarms_ = nullptr;
  TimeManager *time_ = nullptr;
  BuzzerManager *buzzers_ = nullptr;
  DisplayManager *display_ = nullptr;
  LabNames *labNames_ = nullptr;

  UiScreen screen_ = SCR_CLOCK;

  // Generic list-menu cursor state (reused across MAIN_MENU / ALARM_TARGET / ALARM_SLOT /
  // TIME_MENU / DIAG_MENU).
  int8_t cursor_ = 0;

  AlarmTarget editTarget_ = TARGET_LAB1;
  uint8_t editSlotUi_ = 1; // 1-MAX_ALARMS_PER_LAB
  Alarm editBuffer_;
  AlarmEditField editField_ = FIELD_STATUS;
  uint8_t editDayCursor_ = 0; // for custom day toggle screen
  uint8_t editLabelIndex_ = 0;

  // Time settings scratch state.
  DateTime manualEdit_ = DateTime((uint32_t)0);
  uint8_t manualField_ = 0; // 0=Y 1=Mo 2=D 3=H 4=Mi

  void goTo(UiScreen s) { screen_ = s; cursor_ = 0; }

  void renderMainMenu();
  void renderAlarmSlot();
  void renderAlarmEdit();
  void renderAlarmEditDays();
  void renderAlarmEditLabel();
  void renderTimeMenu();
  void renderTimeTz();
  void renderTimeFormat();
  void renderTimeManual();
  void renderDiagMenu();
  void renderDiagGps();
  void renderDiagBrightness();
  void renderDiagVolume();

  void handleMainMenu(ButtonEvent evt);
  void handleAlarmSlot(ButtonEvent evt);
  void handleAlarmEdit(ButtonEvent evt);
  void handleAlarmEditDays(ButtonEvent evt);
  void handleAlarmEditLabel(ButtonEvent evt);
  void handleTimeMenu(ButtonEvent evt);
  void handleTimeTz(ButtonEvent evt);
  void handleTimeFormat(ButtonEvent evt);
  void handleTimeManual(ButtonEvent evt);
  void handleDiagMenu(ButtonEvent evt);
  void handleDiagGps(ButtonEvent evt);
  void handleDiagBrightness(ButtonEvent evt);
  void handleDiagVolume(ButtonEvent evt);
};
