#include "buzzer_manager.h"

#include <math.h>
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"

// Each lab's speaker is one channel of a PAM8403 amplifier, fed an ESP32 LEDC PWM square
// wave at the note's frequency (classic arduino-esp32 v2.x API: ledcSetup/ledcAttachPin/
// ledcWriteTone/ledcWrite). If your platform pulls in arduino-esp32 core 3.x, swap these for
// ledcAttach(pin, freq, res) / ledcWriteTone(pin, freq) — see README "Toolchain Notes".
//
// Volume is the square wave's duty cycle: a pulse of duty d carries sin(pi*d) of the
// full-strength fundamental, peaking at 50%. The volume percent is squared first so the
// slider feels roughly even to the ear (loudness is perceived logarithmically).

// Shared by both labs. Written from any core via BuzzerManager::setVolume(), read by the
// Core-0 Buzzer state machines; a single byte, so a plain volatile is enough.
static volatile uint8_t sVolumePercent = DEFAULT_VOLUME_PERCENT;

static uint32_t dutyForVolume(uint8_t percent) {
  if (percent == 0) return 0;
  float amplitude = (percent / 100.0f) * (percent / 100.0f);
  float duty = asinf(amplitude) / (float)M_PI; // 0..0.5
  uint32_t maxDuty = 1UL << SPEAKER_LEDC_RESOLUTION_BITS;
  uint32_t d = (uint32_t)(duty * maxDuty + 0.5f);
  return d < 1 ? 1 : d;
}

namespace notes {
  constexpr uint16_t REST = 0;
  constexpr uint16_t B3  = 247;
  constexpr uint16_t CS4 = 277;
  constexpr uint16_t D4  = 294;
  constexpr uint16_t E4  = 330;
  constexpr uint16_t FS4 = 370;
  constexpr uint16_t G4  = 392;
  constexpr uint16_t GS4 = 415;
  constexpr uint16_t A4  = 440;
}

// --- Westminster Quarter Chime (simplified 4-note phrase, repeats) ---
static const ChimeNote kWestminster[] = {
  { notes::E4, 500 }, { notes::CS4, 500 }, { notes::D4, 500 }, { notes::B3, 900 },
  { notes::REST, 600 },
};

// --- High-frequency school bell: rapid sharp pulses ---
static const ChimeNote kHighFreqBell[] = {
  { 3200, 150 }, { notes::REST, 100 },
  { 3200, 150 }, { notes::REST, 100 },
  { 3200, 150 }, { notes::REST, 400 },
};

// --- Standard pulsed beep ---
static const ChimeNote kStandardPulse[] = {
  { 1000, 400 }, { notes::REST, 400 },
};

// --- Double chime: two short beeps then a pause ---
static const ChimeNote kDoubleChime[] = {
  { 1500, 200 }, { notes::REST, 150 },
  { 1500, 200 }, { notes::REST, 700 },
};

const ChimeNote *Buzzer::melodyFor(ChimeStyle style, uint8_t &lenOut) {
  switch (style) {
    case CHIME_WESTMINSTER:
      lenOut = sizeof(kWestminster) / sizeof(ChimeNote);
      return kWestminster;
    case CHIME_HIGH_FREQ_BELL:
      lenOut = sizeof(kHighFreqBell) / sizeof(ChimeNote);
      return kHighFreqBell;
    case CHIME_DOUBLE_CHIME:
      lenOut = sizeof(kDoubleChime) / sizeof(ChimeNote);
      return kDoubleChime;
    case CHIME_STANDARD_PULSE:
    default:
      lenOut = sizeof(kStandardPulse) / sizeof(ChimeNote);
      return kStandardPulse;
  }
}

void Buzzer::begin(uint8_t pin, uint8_t ledcChannel) {
  pin_ = pin;
  ledcChannel_ = ledcChannel;
  ledcSetup(ledcChannel_, 2000, SPEAKER_LEDC_RESOLUTION_BITS);
  ledcAttachPin(pin_, ledcChannel_);
  ledcWrite(ledcChannel_, 0);
}

void Buzzer::playNote(uint16_t freqHz) {
  currentFreqHz_ = freqHz;
  if (freqHz == 0) {
    silence();
    return;
  }
  ledcWriteTone(ledcChannel_, freqHz); // sets the pitch (and a 50% duty we replace below)
  applyDuty();
}

void Buzzer::applyDuty() {
  appliedVolume_ = sVolumePercent;
  ledcWrite(ledcChannel_, currentFreqHz_ ? dutyForVolume(appliedVolume_) : 0);
}

// Duty 0 holds the pin at a steady 0V, so the amp's input sees no signal and stays quiet.
void Buzzer::silence() {
  currentFreqHz_ = 0;
  ledcWriteTone(ledcChannel_, 0);
  ledcWrite(ledcChannel_, 0);
}

void Buzzer::start(ChimeStyle style, uint16_t durationSec) {
  melody_ = melodyFor(style, melodyLen_);
  noteIndex_ = 0;
  ringStartMs_ = millis();
  noteStartMs_ = ringStartMs_;
  durationMs_ = (uint32_t)durationSec * 1000UL;
  ringing_ = true;
  playNote(melody_[0].freqHz);
}

void Buzzer::stop() {
  ringing_ = false;
  silence();
}

void Buzzer::update() {
  if (!ringing_) return;

  unsigned long nowMs = millis();

  if (durationMs_ != 0 && (nowMs - ringStartMs_) >= durationMs_) {
    stop();
    return;
  }

  if ((nowMs - noteStartMs_) >= melody_[noteIndex_].durationMs) {
    noteIndex_ = (noteIndex_ + 1) % melodyLen_;
    noteStartMs_ = nowMs;
    playNote(melody_[noteIndex_].freqHz);
  } else if (appliedVolume_ != sVolumePercent) {
    applyDuty(); // volume changed mid-note (e.g. slider dragged during a test tone)
  }
}

void BuzzerManager::begin() {
  requestMutex_ = xSemaphoreCreateMutex();
  prefs_.begin(NVS_NAMESPACE, false);
  uint8_t v = prefs_.getUChar("volume", DEFAULT_VOLUME_PERCENT);
  prefs_.end();
  sVolumePercent = v > 100 ? 100 : v;
  lab1_.begin(PIN_SPEAKER_LAB1, SPEAKER_LEDC_CHANNEL_LAB1);
  lab2_.begin(PIN_SPEAKER_LAB2, SPEAKER_LEDC_CHANNEL_LAB2);
}

void BuzzerManager::setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  sVolumePercent = percent;
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putUChar("volume", percent);
  prefs_.end();
}

uint8_t BuzzerManager::volume() const { return sVolumePercent; }

void BuzzerManager::requestTestBeep(uint8_t labIndex) {
  xSemaphoreTake(requestMutex_, portMAX_DELAY);
  testBeepRequestLab_ = (int8_t)labIndex;
  xSemaphoreGive(requestMutex_);
}

// Wiring self-test, run with each test tone: reads the speaker pin's actual pad level a few
// thousand times while the tone plays and logs how often it was high. ~the expected duty
// with plenty of edges = the ESP32 side is fine, so silence is in the divider/amp/speaker;
// stuck at 0% or 100% = the pin is shorted or wired to the wrong place.
static void logPinActivity(uint8_t labNum, uint8_t pin) {
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]); // lets GPIO_IN_REG see the pad while LEDC drives it
  const uint16_t kSamples = 4000;
  uint16_t high = 0, edges = 0;
  int last = -1;
  unsigned long t0 = micros();
  for (uint16_t i = 0; i < kSamples; i++) {
    int v = (REG_READ(GPIO_IN_REG) >> pin) & 1;
    high += v;
    if (last >= 0 && v != last) edges++;
    last = v;
    delayMicroseconds(5);
  }
  unsigned long us = micros() - t0;
  float expected = 100.0f * dutyForVolume(sVolumePercent) / (1UL << SPEAKER_LEDC_RESOLUTION_BITS);
  Serial.printf("[Speaker] Test %u: GPIO%u high %.1f%% (expect ~%.1f%%), %u edges in %lu ms "
                "(expect ~%lu for the 1 kHz tone)\n",
                labNum, pin, 100.0f * high / kSamples, expected, edges, us / 1000, us / 500);
}

void BuzzerManager::serviceRequests() {
  xSemaphoreTake(requestMutex_, portMAX_DELAY);
  int8_t lab = testBeepRequestLab_;
  testBeepRequestLab_ = -1;
  xSemaphoreGive(requestMutex_);

  // A short, non-blocking chime via the existing state machine — reusing start() keeps this
  // on the same Core-0-only path as a real alarm instead of adding a second, blocking way to
  // drive the hardware.
  if (lab == 0) {
    lab1_.start(CHIME_STANDARD_PULSE, 1);
    logPinActivity(1, lab1_.pin());
  } else if (lab == 1) {
    lab2_.start(CHIME_STANDARD_PULSE, 1);
    logPinActivity(2, lab2_.pin());
  }
}

void BuzzerManager::update() {
  lab1_.update();
  lab2_.update();
}

void BuzzerManager::stopAll() {
  lab1_.stop();
  lab2_.stop();
}
