#include "lab_names.h"

static const char *kLabNameKeys[2] = {"lab1name", "lab2name"};
static const char *kLabNameDefaults[2] = {LAB1_FOOTER_TEXT, LAB2_FOOTER_TEXT};

void LabNames::begin() {
  mutex_ = xSemaphoreCreateMutex();
  prefs_.begin(NVS_NAMESPACE, false);
  for (uint8_t i = 0; i < 2; i++) {
    String s = prefs_.getString(kLabNameKeys[i], kLabNameDefaults[i]);
    strlcpy(names_[i], s.length() ? s.c_str() : kLabNameDefaults[i], sizeof(names_[i]));
  }
  prefs_.end();
}

void LabNames::get(uint8_t labIndex, char *out, size_t outLen) const {
  if (outLen == 0) return;
  if (labIndex > 1) { out[0] = '\0'; return; }
  xSemaphoreTake(mutex_, portMAX_DELAY);
  strlcpy(out, names_[labIndex], outLen);
  xSemaphoreGive(mutex_);
}

bool LabNames::set(uint8_t labIndex, const char *name) {
  if (labIndex > 1 || !name) return false;

  // Trim surrounding whitespace and drop non-printable characters (the panel font is ASCII).
  char clean[LAB_NAME_MAX_LEN + 1];
  size_t n = 0;
  while (*name == ' ') name++;
  for (; *name && n < LAB_NAME_MAX_LEN; name++) {
    if (*name >= 0x20 && *name < 0x7F) clean[n++] = *name;
  }
  while (n > 0 && clean[n - 1] == ' ') n--;
  clean[n] = '\0';
  if (n == 0) return false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  strlcpy(names_[labIndex], clean, sizeof(names_[labIndex]));
  xSemaphoreGive(mutex_);

  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putString(kLabNameKeys[labIndex], clean);
  prefs_.end();
  return true;
}
