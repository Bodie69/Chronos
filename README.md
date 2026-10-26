# Dual Comlab Matrix Alarm System

Industrial-grade, precision-synchronized dual-room laboratory clock and bell system for
**Computer Lab 1** and **Computer Lab 2**, built on a classic **ESP32 WROOM-32**, chained
**HUB75** RGB matrix panels (256x64 combined, 128x64 per lab), a PAM8403 amplifier driving one speaker per lab, a
**DS3231** RTC, **NEO-6M/7M/8M** GPS time sync, and 4-button on-screen navigation.

## Hardware Wiring

### 1. HUB75 Panel Chain

Both labs' panels are wired as **one continuous HUB75 daisy chain**:

```
ESP32  --(16-pin IDC)-->  [Comlab 1 Panel(s)] DATA-IN
                           [Comlab 1 Panel(s)] DATA-OUT --> [Comlab 2 Panel(s)] DATA-IN
```

- If you have **two native 128x64 panels** (default config): chain length = 2.
- If you have **four 64x64 panels** (two per lab, placed side-by-side): set
  `USE_64X64_QUAD_PANELS 1` in `include/config.h`. Physically arrange all four panels in a
  single left-to-right row in chain order (Lab1-Left, Lab1-Right, Lab2-Left, Lab2-Right) —
  this firmware treats the chain as one flat wide canvas and does not use a serpentine/
  stacked virtual-panel mapping.

All 14 HUB75 signal pins (R1/G1/B1/R2/G2/B2/A/B/C/D/E/LAT/OE/CLK), both speaker pins, the
DS3231 I2C bus, the GPS UART, and all 4 buttons are defined in `include/config.h`.

| Peripheral | Signal | ESP32 GPIO |
| :--- | :--- | :--- |
| HUB75 | R1 / G1 / B1 | 25 / 26 / 27 |
| HUB75 | R2 / G2 / B2 | 14 / 12 / 13 |
| HUB75 | A / B / C / D / E | 19 / 5 / 33 / 18 / 23 |
| HUB75 | LAT / OE / CLK | 15 / 32 / 4 |
| Speaker 1 (Comlab 1) → PAM8403 L in | PWM | 2 |
| Speaker 2 (Comlab 2) → PAM8403 R in | PWM | 16 |
| DS3231 RTC | SDA / SCL | 21 / 22 |
| GPS (UART1) | RX (ESP) / TX (ESP) | 34 / 17 |
| Buttons | UP / DOWN / SELECT / BACK | 35 / 36 / 39 / 0 |

**Why this layout looks unusual:** the HUB75 pins are this board's fixed wiring, given
as-is. Fitting everything else around them on a classic ESP32 (unlike the S3 this project
started on) runs into real constraints: GPIO 6-11 are permanently reserved for the internal
SPI flash, GPIO 1/3 are UART0 (Serial/programming), and GPIO 34/35/36/39 are **input-only**
(no internal pull-up/down, and they can't drive an output). After the 14 HUB75 pins there
were only 9 GPIOs left at all, which is just barely enough:

- **Speaker outputs** landed on GPIO 2 and 16, feeding the PAM8403's L and R inputs. Wire
  each through a divider so the 3.3V PWM signal doesn't clip the amp's input:
  `GPIO --10k--+-- PAM8403 input`, with `+--1k-- GND` at the input (an optional 10nF from
  input to GND smooths the tone). Tie the PAM8403's GND to the ESP32's GND and power it from
  5V. GPIO2 is a boot-strapping pin; the 1k to GND holds it low at reset, which is the state
  it needs to boot.
- **GPS RX** landed on GPIO34 (input-only) since UART RX is pure input and can go on any
  GPIO via the ESP32's GPIO matrix; GPS TX needed a real output pin (17).
- **UP / DOWN / SELECT sit on input-only GPIOs 35/36/39** — since those have no internal
  pull resistor, `INPUT_PULLUP` in `button_manager.cpp` does nothing on them. **You need an
  external ~10kOhm pull-up resistor to 3.3V on each of these three buttons**, or they'll
  float and trigger randomly.
- **BACK is on GPIO0**, which does support the internal pull-up (same mechanism as this
  board's own BOOT button). Minor quirk: holding BACK while powering on will put the board
  into download mode instead of booting normally — same as holding BOOT.

If you change panels/boards again, re-derive this from scratch rather than assuming these
pins port cleanly — they're tightly fitted to what a bare WROOM-32 has left over.

### 2. Power

- HUB75 panels draw significant current (multiple amps at full white/brightness) — power
  them from a dedicated 5V supply sized for your panel count, **not** from the ESP32's
  onboard regulator. Share ground between the panel PSU and the ESP32.
- The DS3231 module's coin-cell battery keeps the RTC running through power loss.

## Clock Screen Layout

Each 128x64 lab viewport is a fixed 3-row stack (top → bottom). The header and footer text
are configured in `include/config.h` (`HEADER_MARQUEE_TEXT`, `HEADER_MARQUEE_SPEED_PX_PER_SEC`,
`LAB1_FOOTER_TEXT`, `LAB2_FOOTER_TEXT`):

```
+----------------------------------------------------------------+
|<== INFORMATION TECHNOLOGY                                      |  <- header, size 2, yellow, scrolling
|                                                                 |
|                        12:30                                   |  <- time, size 4, red
|                                                                 |
|                      COMLAB 1                            . .   |  <- footer, size 2, blue
+----------------------------------------------------------------+
```

The header is a continuously scrolling marquee showing the **same** text
(`"INFORMATION TECHNOLOGY"`) on **both** panels in lockstep, since it's too wide to fit
statically at size 2 on a 128px panel. Comlab 1's footer reads `COMLAB 1`, Comlab 2's reads
`COMLAB 2`. The two small dots — GPS-lock and RTC-present status — sit in the footer row's
top-right corner, since the header row has scrolling text passing through it continuously
and would periodically cover fixed dots parked there.

**No AM/PM indicator**: the time digits themselves still respect the 12h/24h format setting
(e.g. `7:30` in 12h mode vs `19:30` in 24h mode), but AM/PM is not displayed anywhere on the
clock screen by design — `TimeManager::formatTimeParts()` still computes it internally
(`main.cpp` just doesn't forward it to `LabScreenData`) in case a future screen wants it back.

**Scroll clipping note:** the two lab panels are one continuous physical HUB75 chain (one
shared framebuffer), not two independent displays — a naive scrolling implementation would
let characters mid-transit across a panel's left/right edge bleed a few pixels onto the
*other* panel. `printMarquee()` in `src/display_manager.cpp` avoids this by drawing
character-by-character and skipping any glyph cell not **entirely** inside its own lab's
128px window, rather than relying on `Adafruit_GFX`'s cursor/print (which has no per-region
clip rect here). If you change panel width/topology, keep this in mind for any new scrolling
element.

Seconds are intentionally dropped from the time — an `HH:MM:SS` string doesn't fit at size 4
on a 128px-wide panel; the full precision is still tracked internally and available via
`TimeManager::formatTime()`.

## Firmware Architecture

- **Core 0** (`timeAlarmTask`): GPS NMEA parsing, RTC read/write, alarm-schedule evaluation,
  and both speakers' non-blocking chime state machines. This is the time-critical path and
  never touches the display.
- **Core 1** (`uiDisplayTask`): button polling/debounce, menu navigation, and all HUB75 DMA
  rendering (double-buffered — a full frame is drawn off-screen then flipped).
- Shared state (which lab's alarm is currently ringing, and its label) is protected by a
  FreeRTOS mutex.

### Time Sync Priority

1. **GPS lock** (3+ satellites, valid date/time/HDOP) is authoritative whenever available,
   and periodically re-syncs the DS3231 RTC (every 5 minutes while locked).
2. **DS3231 RTC** is used whenever GPS has no fix (e.g. indoors) — accurate to a few seconds
   per month.
3. **Manual entry** via the button menu is the last resort if both GPS and the RTC battery
   are unavailable.

### Alarms

- 20 slots each for Comlab 1 and Comlab 2 (40 total), stored in flash
  (`Preferences`/NVS) and surviving power loss.
- Each alarm: enabled flag, time (HH:MM), day mask (individually toggleable Sun-Sat, with
  quick-cycle Weekdays/Daily presets), ring duration (5/10/15/30s or "until dismissed"),
  chime style, and a label chosen from class-period presets.
- Comlab 1 and Comlab 2 alarms ring completely independently — a Comlab 1 alarm firing does
  not sound Speaker 2, and vice versa. To ring both labs at once, set the same alarm on each.
- Pressing **BACK/STOP** while any alarm is ringing silences it immediately (both labs, if
  both are active).

### Buttons

| Button | GPIO | Function |
| :--- | :--- | :--- |
| UP | 35 * | Navigate up / increment value (auto-repeats when held) |
| DOWN | 36 * | Navigate down / decrement value (auto-repeats when held) |
| SELECT | 39 * | Enter menu / confirm / save |
| BACK/STOP | 0 | Back / exit menu / silence a ringing alarm |

\* Input-only GPIO — needs an external ~10kOhm pull-up resistor to 3.3V (see the HUB75
wiring section above).

From the clock screen, **SELECT** opens the main menu:
`Comlab 1 Alarms → Comlab 2 Alarms → Time & Date → Diagnostics → Back to Clock`.
Comlab 1 and Comlab 2 each have their own top-level entry straight into that lab's 20 alarm
slots — there's no shared "manage alarms" step to pick a lab from first.

## Toolchain Notes

- `platformio.ini` targets `board = esp32dev` (`espressif32` platform, `framework = arduino`)
  — a generic classic ESP32 WROOM-32 devkit. This board has no PSRAM and no native USB: it
  flashes and logs over an external USB-UART bridge chip (typically a CP2102/CH340,
  enumerating as a normal COM port) — no USB-CDC build flags are needed, unlike an ESP32-S3.
- The speaker driver (`src/buzzer_manager.cpp`) plays PWM square-wave tones, with volume set
  by the duty cycle (0-100%, saved; set it under Diagnostics → Volume on the panel or the web
  dashboard). It uses the classic **arduino-esp32 v2.x** LEDC tone API (`ledcSetup` /
  `ledcAttachPin` / `ledcWriteTone` / `ledcWrite`). If your
  resolved platform pulls in **arduino-esp32 core 3.x**, these calls have been replaced with
  a pin-based API (`ledcAttach(pin, freq, resolution)` / `ledcWriteTone(pin, freq)`); update
  `Buzzer::begin()` / `playNote()` / `silence()` accordingly if `pio run` reports them
  missing.
- The HUB75 driver library (`ESP32-HUB75-MatrixPanel-DMA` by mrfaptastic) is pulled directly
  from GitHub in `lib_deps` rather than a pinned registry version, since its API has moved
  quickly across releases. It auto-detects "original ESP32" at compile time from the target
  chip, using its I2S-DMA driver rather than the S3-specific GDMA/LCD-parallel one.

## Build & Flash

```sh
pio run                             # compile
pio run -t upload --upload-port COMx   # flash (adjust COMx to your board)
pio device monitor -p COMx -b 115200   # serial log
```

A CP2102/CH340-based board normally auto-resets into the bootloader on upload via its DTR/RTS
lines, so a manual BOOT-button press usually isn't needed (unlike some bare ESP32-S3 native-
USB boards). If `pio run -t upload` reports a boot-mode/connection error anyway, hold BOOT,
tap RESET, release BOOT, then retry.

## Verification Checklist

- [ ] `pio run` compiles with zero errors/warnings.
- [ ] Both lab viewports render independently and show the correct clock/date.
- [ ] GPS satellite icon lights green only once 3+ sats are locked; RTC icon reflects
      DS3231 presence.
- [ ] Setting an alarm on Comlab 1 only plays on Speaker 1, and Comlab 2 only on Speaker 2.
- [ ] Power-cycling the board preserves all saved alarms and time/format settings.
- [ ] BACK/STOP silences a ringing alarm from the clock screen.
- [ ] Manual time entry works when GPS is unavailable and the RTC battery is dead/missing.
