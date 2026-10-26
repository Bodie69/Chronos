#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// One melody note: frequency in Hz (0 = silence/rest) and duration in ms.
struct ChimeNote {
  uint16_t freqHz;
  uint16_t durationMs;
};

// Independent, non-blocking chime player for one lab's speaker (one PAM8403 channel). Call
// update() every loop. (Named Buzzer from when the outputs drove piezo buzzers.)
class Buzzer {
public:
  void begin(uint8_t pin, uint8_t ledcChannel);

  // Starts ringing with the given chime style. durationSec == 0 means ring until stop() is
  // called (e.g. BACK/STOP button). The melody loops for the whole duration.
  void start(ChimeStyle style, uint16_t durationSec);
  void stop();
  void update();

  bool isRinging() const { return ringing_; }
  uint8_t pin() const { return pin_; }

private:
  uint8_t pin_ = 0;
  uint8_t ledcChannel_ = 0;
  bool ringing_ = false;
  uint16_t currentFreqHz_ = 0;
  uint8_t appliedVolume_ = 0; // volume the current note was started at

  const ChimeNote *melody_ = nullptr;
  uint8_t melodyLen_ = 0;
  uint8_t noteIndex_ = 0;
  unsigned long noteStartMs_ = 0;

  unsigned long ringStartMs_ = 0;
  uint32_t durationMs_ = 0; // 0 = infinite until stop()

  void playNote(uint16_t freqHz);
  void applyDuty();
  void silence();
  const ChimeNote *melodyFor(ChimeStyle style, uint8_t &lenOut);
};

// Owns the two lab speakers and the shared volume setting. The Buzzer objects
// are Core-0-owned (see main.cpp's Core 0/Core 1 split); every other method below must only
// be called from Core 0 (timeAlarmTask). Core 1 (the UI task/menu) may only call
// requestTestBeep(), which hands off to Core 0 via serviceRequests() instead of touching the
// Buzzer objects directly.
class BuzzerManager {
public:
  void begin();
  void update();

  Buzzer &lab1() { return lab1_; }
  Buzzer &lab2() { return lab2_; }

  void startLab1(ChimeStyle style, uint16_t durationSec) { lab1_.start(style, durationSec); }
  void startLab2(ChimeStyle style, uint16_t durationSec) { lab2_.start(style, durationSec); }
  void stopAll();

  // Thread-safe: callable from Core 1 to request a short diagnostic test tone on the given
  // lab (0 or 1). Only records the request; serviceRequests() (Core 0) does the actual
  // speaker start.
  void requestTestBeep(uint8_t labIndex);
  // Thread-safe: callable from any core. Takes effect immediately, even mid-note, and
  // persists to NVS.
  void setVolume(uint8_t percent); // 0-100
  uint8_t volume() const;
  // Call once per loop tick from Core 0, alongside update().
  void serviceRequests();

private:
  Buzzer lab1_;
  Buzzer lab2_;

  SemaphoreHandle_t requestMutex_ = nullptr;
  Preferences prefs_;
  int8_t testBeepRequestLab_ = -1; // -1 = none, else 0/1
};
