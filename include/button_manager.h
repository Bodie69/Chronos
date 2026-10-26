#pragma once

#include <Arduino.h>
#include "config.h"

enum ButtonEvent : uint8_t {
  BTN_EVENT_NONE = 0,
  BTN_EVENT_UP,
  BTN_EVENT_DOWN,
  BTN_EVENT_SELECT,
  BTN_EVENT_BACK
};

// Debounced, non-blocking 4-button reader. UP/DOWN auto-repeat while held (for fast value
// scrolling in menus); SELECT/BACK fire once per press.
//
// The pins are scanned by a dedicated high-priority task every BUTTON_SCAN_INTERVAL_MS and
// events are queued, so presses are never lost while the UI task is busy rendering (the UI
// loop runs at only ~27Hz, too slow to reliably catch a quick tap on its own).
class ButtonManager {
public:
  // Configures the pins and starts the background scan task.
  void begin();

  // Call every UI loop. Returns the next queued event (or BTN_EVENT_NONE), never blocks.
  ButtonEvent poll();

  // True when the event last returned by poll() came from auto-repeat (button held) rather
  // than a fresh press. Menu lists ignore these so they only move on real presses.
  bool lastEventWasRepeat() const { return lastWasRepeat_; }

private:
  struct ButtonState {
    uint8_t pin;
    bool lastRaw = true;      // pulled up = HIGH when not pressed
    bool stableState = true;
    unsigned long lastChangeMs = 0;
    bool pressed = false;     // debounced logical "currently held"
    unsigned long pressedAtMs = 0;
    unsigned long lastRepeatMs = 0;
    bool armed = false;       // becomes true once seen released; guards floating/stuck pins
    bool lastFireWasRepeat = false;
  };

  struct QueuedEvent {
    ButtonEvent evt;
    bool repeat;
  };

  ButtonState up_, down_, select_, back_;
  bool lastWasRepeat_ = false;
  QueueHandle_t queue_ = nullptr;

  static void scanTask(void *arg);
  void scan();
  void enqueue(ButtonEvent evt, bool repeat);

  // Returns true exactly once per new press (edge), or true again each time the auto-repeat
  // interval elapses while held (only when allowRepeat is set for that button).
  bool updateButton(ButtonState &b, bool allowRepeat);
};
