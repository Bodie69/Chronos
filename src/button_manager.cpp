#include "button_manager.h"

void ButtonManager::begin() {
  up_.pin = PIN_BTN_UP;
  down_.pin = PIN_BTN_DOWN;
  select_.pin = PIN_BTN_SELECT;
  back_.pin = PIN_BTN_BACK;

  pinMode(up_.pin, INPUT_PULLUP);
  pinMode(down_.pin, INPUT_PULLUP);
  pinMode(select_.pin, INPUT_PULLUP);
  pinMode(back_.pin, INPUT_PULLUP);

  // Seed state from the real pin levels so a pin that already reads LOW at boot (e.g. a
  // GPIO35/36/39 button missing its external pull-up, which floats) isn't taken as a press.
  for (ButtonState *b : { &up_, &down_, &select_, &back_ }) {
    bool raw = digitalRead(b->pin);
    b->lastRaw = raw;
    b->stableState = raw;
    b->armed = (raw == HIGH);
  }
  Serial.printf("[Button] boot levels UP=%d DOWN=%d SELECT=%d BACK=%d (1=released)\n",
                up_.lastRaw, down_.lastRaw, select_.lastRaw, back_.lastRaw);

  queue_ = xQueueCreate(BUTTON_EVENT_QUEUE_LEN, sizeof(QueuedEvent));
  // Priority 2 (above the UI task's 1) on the UI core, so scanning preempts rendering.
  xTaskCreatePinnedToCore(scanTask, "ButtonScan", 3072, this, 2, nullptr, 1);
}

void ButtonManager::scanTask(void *arg) {
  ButtonManager *self = static_cast<ButtonManager *>(arg);
  for (;;) {
    self->scan();
    vTaskDelay(pdMS_TO_TICKS(BUTTON_SCAN_INTERVAL_MS));
  }
}

void ButtonManager::enqueue(ButtonEvent evt, bool repeat) {
  QueuedEvent q = { evt, repeat };
  // Never block the scanner: if the UI has fallen behind, drop the event (only happens
  // with auto-repeat piling up while held).
  if (xQueueSend(queue_, &q, 0) == pdTRUE && !repeat) {
    static const char *const kNames[] = { "NONE", "UP", "DOWN", "SELECT", "BACK" };
    Serial.printf("[Button] %s\n", kNames[evt]);
  }
}

bool ButtonManager::updateButton(ButtonState &b, bool allowRepeat) {
  bool raw = digitalRead(b.pin); // HIGH = released, LOW = pressed (active LOW)
  unsigned long nowMs = millis();

  if (raw != b.lastRaw) {
    b.lastChangeMs = nowMs;
    b.lastRaw = raw;
  }

  bool firedEvent = false;
  b.lastFireWasRepeat = false;

  if ((nowMs - b.lastChangeMs) >= BUTTON_DEBOUNCE_MS && b.stableState != raw) {
    b.stableState = raw;
    if (raw == HIGH) b.armed = true;
    if (raw == LOW && b.armed) {
      // Fresh press edge.
      b.pressed = true;
      b.pressedAtMs = nowMs;
      b.lastRepeatMs = nowMs;
      firedEvent = true;
    } else {
      b.pressed = false;
    }
  }

  if (!firedEvent && allowRepeat && b.pressed) {
    unsigned long heldMs = nowMs - b.pressedAtMs;
    if (heldMs >= BUTTON_REPEAT_DELAY_MS) {
      if ((nowMs - b.lastRepeatMs) >= BUTTON_REPEAT_INTERVAL_MS) {
        b.lastRepeatMs = nowMs;
        firedEvent = true;
        b.lastFireWasRepeat = true;
      }
    }
  }

  return firedEvent;
}

void ButtonManager::scan() {
  // Update every button each tick and queue every one that fired, so simultaneous presses
  // are all delivered rather than only the highest-priority one.
  if (updateButton(select_, false)) enqueue(BTN_EVENT_SELECT, false);
  if (updateButton(back_, false)) enqueue(BTN_EVENT_BACK, false);
  if (updateButton(up_, true)) enqueue(BTN_EVENT_UP, up_.lastFireWasRepeat);
  if (updateButton(down_, true)) enqueue(BTN_EVENT_DOWN, down_.lastFireWasRepeat);
}

ButtonEvent ButtonManager::poll() {
  QueuedEvent q;
  if (queue_ && xQueueReceive(queue_, &q, 0) == pdTRUE) {
    lastWasRepeat_ = q.repeat;
    return q.evt;
  }
  lastWasRepeat_ = false;
  return BTN_EVENT_NONE;
}
