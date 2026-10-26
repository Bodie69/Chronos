#include <Arduino.h>
#include "config.h"
#include "time_manager.h"
#include "alarm_manager.h"
#include "buzzer_manager.h"
#include "button_manager.h"
#include "display_manager.h"
#include "ui_menu.h"
#include "lab_names.h"
#include "web_dashboard.h"

// Dual-core split:
//   Core 0 ("TimeAlarmTask"): GPS/RTC time engine, alarm scheduling, buzzer state machines.
//                             This is the time-critical path and must never block on display I/O.
//   Core 1 ("UiDisplayTask"): Button polling, menu navigation, HUB75 DMA rendering.
// Shared state (current time snapshot + per-lab alarm-active flags) is protected by a mutex.

TimeManager timeManager;
AlarmManager alarmManager;
BuzzerManager buzzerManager;
ButtonManager buttonManager;
DisplayManager displayManager;
UiMenu uiMenu;
LabNames labNames;
WebDashboard webDashboard;

static SemaphoreHandle_t stateMutex;

struct SharedState {
  bool lab1AlarmActive = false;
  bool lab2AlarmActive = false;
  char lab1ActiveLabel[ALARM_LABEL_MAX_LEN] = "";
  char lab2ActiveLabel[ALARM_LABEL_MAX_LEN] = "";
  // Set by uiDisplayTask (Core 1) on BACK/STOP, consumed by timeAlarmTask (Core 0). This is
  // the only cross-core link into buzzerManager: the Buzzer state machines (ringing_,
  // melody_, noteIndex_, ...) are owned by Core 0 and are not themselves mutex-protected, so
  // Core 1 must never call buzzerManager methods directly — doing so races Core 0's own
  // update()/startLabN() calls on the same unguarded fields.
  bool dismissRequested = false;
} shared;

// -----------------------------------------------------------------------------------------
// Core 0: Time engine + alarm scheduler + buzzers
// -----------------------------------------------------------------------------------------
void timeAlarmTask(void *pvParameters) {
  int8_t lastCheckedMinute = -1;

  for (;;) {
    timeManager.update();
    buzzerManager.update();
    // Serial console test hooks: send '1' or '2' to play that lab's speaker test tone.
    while (Serial.available()) {
      int c = Serial.read();
      if (c == '1') buzzerManager.requestTestBeep(0);
      else if (c == '2') buzzerManager.requestTestBeep(1);
    }
    buzzerManager.serviceRequests();

    DateTime nowLocal = timeManager.now();

    if (nowLocal.minute() != lastCheckedMinute) {
      lastCheckedMinute = nowLocal.minute();

      uint8_t dayBit = 1 << nowLocal.dayOfTheWeek();
      uint32_t nowMinuteToken = (uint32_t)(nowLocal.unixtime() / 60);
      AlarmTrigger trigger;
      if (alarmManager.checkDue(nowLocal.hour(), nowLocal.minute(), dayBit, nowMinuteToken, trigger)) {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        if (trigger.fireLab1) {
          buzzerManager.startLab1(trigger.alarmLab1.chime, trigger.alarmLab1.durationSec);
          shared.lab1AlarmActive = true;
          strncpy(shared.lab1ActiveLabel, trigger.alarmLab1.label, ALARM_LABEL_MAX_LEN);
          Serial.printf("[Alarm] FIRE Lab1: %s (chime=%u dur=%us)\n", trigger.alarmLab1.label,
                        trigger.alarmLab1.chime, trigger.alarmLab1.durationSec);
        }
        if (trigger.fireLab2) {
          buzzerManager.startLab2(trigger.alarmLab2.chime, trigger.alarmLab2.durationSec);
          shared.lab2AlarmActive = true;
          strncpy(shared.lab2ActiveLabel, trigger.alarmLab2.label, ALARM_LABEL_MAX_LEN);
          Serial.printf("[Alarm] FIRE Lab2: %s (chime=%u dur=%us)\n", trigger.alarmLab2.label,
                        trigger.alarmLab2.chime, trigger.alarmLab2.durationSec);
        }
        xSemaphoreGive(stateMutex);
      }
    }

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool doDismiss = shared.dismissRequested;
    shared.dismissRequested = false;
    xSemaphoreGive(stateMutex);
    if (doDismiss) {
      buzzerManager.stopAll();
      Serial.println("[Alarm] Dismissed via BACK/STOP button");
    }

    static unsigned long lastStatusLogMs = 0;
    if (millis() - lastStatusLogMs >= 10000) {
      lastStatusLogMs = millis();
      Serial.printf("[Status] %02d:%02d:%02d  GPS=%s sats=%u  RTC=%s  DMA free=%u min=%u\n",
                    nowLocal.hour(), nowLocal.minute(), nowLocal.second(),
                    timeManager.isGpsLocked() ? "LOCK" : "searching",
                    timeManager.satelliteCount(),
                    timeManager.isRtcPresent() ? "ok" : "MISSING",
                    heap_caps_get_free_size(MALLOC_CAP_DMA),
                    heap_caps_get_minimum_free_size(MALLOC_CAP_DMA));
    }

    // Clear the "active" banner once the buzzer for that lab finishes ringing (either timed
    // out or was dismissed via BTN_BACK on the UI task).
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (shared.lab1AlarmActive && !buzzerManager.lab1().isRinging()) shared.lab1AlarmActive = false;
    if (shared.lab2AlarmActive && !buzzerManager.lab2().isRinging()) shared.lab2AlarmActive = false;
    xSemaphoreGive(stateMutex);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// -----------------------------------------------------------------------------------------
// Core 1: Buttons + menu + HUB75 rendering
// -----------------------------------------------------------------------------------------
void uiDisplayTask(void *pvParameters) {
  for (;;) {
    ButtonEvent evt = buttonManager.poll();

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool anyActive = shared.lab1AlarmActive || shared.lab2AlarmActive;
    xSemaphoreGive(stateMutex);

    if (anyActive) {
      if (evt == BTN_EVENT_BACK) {
        // Only request the dismiss here; buzzerManager itself is Core-0-owned and must only
        // be touched from timeAlarmTask (see SharedState::dismissRequested).
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        shared.dismissRequested = true;
        xSemaphoreGive(stateMutex);
      }
      // All other button events are ignored while an alarm is actively ringing.
    } else {
      uiMenu.handleEvent(evt, buttonManager.lastEventWasRepeat());
    }

    if (uiMenu.isOnClockScreen()) {
      displayManager.clear();

      // AM/PM is intentionally not shown on the clock screen; formatTimeParts() is still
      // used (not formatTime()) because its "hm" output already respects the 12h/24h
      // format setting (e.g. "7:30" vs "19:30") without the seconds that don't fit at size 4.
      char hmBuf[8], ampmBuf[4];
      timeManager.formatTimeParts(hmBuf, sizeof(hmBuf), ampmBuf, sizeof(ampmBuf));
      bool gpsLocked = timeManager.isGpsLocked();
      bool rtcPresent = timeManager.isRtcPresent();

      for (uint8_t lab = 0; lab < 2; lab++) {
        char labName[LAB_NAME_MAX_LEN + 1];
        labNames.get(lab, labName, sizeof(labName));
        LabScreenData data;
        data.footerText = labName;
        data.timeHM = hmBuf;
        data.gpsLocked = gpsLocked;
        data.rtcPresent = rtcPresent;

        displayManager.drawClockScreen(lab, data);
      }

      bool flashOn = (millis() / 300) % 2 == 0;
      xSemaphoreTake(stateMutex, portMAX_DELAY);
      bool lab1Active = shared.lab1AlarmActive;
      bool lab2Active = shared.lab2AlarmActive;
      char lab1Label[ALARM_LABEL_MAX_LEN], lab2Label[ALARM_LABEL_MAX_LEN];
      strncpy(lab1Label, shared.lab1ActiveLabel, ALARM_LABEL_MAX_LEN);
      strncpy(lab2Label, shared.lab2ActiveLabel, ALARM_LABEL_MAX_LEN);
      xSemaphoreGive(stateMutex);

      if (lab1Active) displayManager.drawAlarmActive(0, lab1Label, flashOn);
      if (lab2Active) displayManager.drawAlarmActive(1, lab2Label, flashOn);

      displayManager.show();
    } else {
      displayManager.clear();
      uiMenu.render();
      displayManager.show();
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("=== Dual Comlab Matrix Alarm System ===");

  stateMutex = xSemaphoreCreateMutex();

  bool rtcOk = timeManager.begin();
  Serial.printf("[TimeManager] DS3231 RTC: %s\n", rtcOk ? "FOUND" : "NOT FOUND");

  labNames.begin();

  alarmManager.begin();
  Serial.println("[AlarmManager] Alarms loaded from NVS");

  buzzerManager.begin();
  Serial.println("[BuzzerManager] Buzzer PWM channels initialized");

  buttonManager.begin();
  Serial.println("[ButtonManager] Buttons initialized");

  bool dispOk = displayManager.begin();
  Serial.printf("[DisplayManager] HUB75 DMA init: %s\n", dispOk ? "OK" : "FAILED");

  uiMenu.begin(&alarmManager, &timeManager, &buzzerManager, &displayManager, &labNames);

  // After the display so the HUB75 DMA buffers get first pick of internal RAM.
  webDashboard.begin(&alarmManager, &timeManager, &labNames, &buzzerManager, &displayManager);
  Serial.printf("[Setup] Free heap: %u bytes\n", ESP.getFreeHeap());

  xTaskCreatePinnedToCore(timeAlarmTask, "TimeAlarmTask", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(uiDisplayTask, "UiDisplayTask", 8192, nullptr, 1, nullptr, 1);

  Serial.println("[Setup] Ready.");
}

void loop() {
  // All work happens in the two pinned FreeRTOS tasks above.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
