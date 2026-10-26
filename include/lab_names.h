#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Runtime-editable display names for the two labs ("COMLAB 1" / "COMLAB 2" by default),
// shown in each panel's footer and the button menus, and editable from the web dashboard.
// Thread-safe: the web server task writes while the Core-1 UI task reads every frame.
class LabNames {
public:
  void begin();

  // Copies lab 0/1's name into out (always NUL-terminated).
  void get(uint8_t labIndex, char *out, size_t outLen) const;

  // Trims and stores the name (persists to NVS). Returns false if it ends up empty.
  bool set(uint8_t labIndex, const char *name);

private:
  char names_[2][LAB_NAME_MAX_LEN + 1];
  Preferences prefs_;
  mutable SemaphoreHandle_t mutex_ = nullptr;
};
