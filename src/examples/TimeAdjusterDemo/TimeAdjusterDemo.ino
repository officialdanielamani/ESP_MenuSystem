// TimeAdjusterDemo.ino
// Demonstrates all TimeAdjuster configurations described in TimeAdjuster.h
//
// Hardware: 128×64 OLED (SSD1306), three push-buttons (Up / Down / OK)
// ─────────────────────────────────────────────────────────────────────────────
#include <U8g2lib.h>

// This sketch only uses the time editor, so every other optional widget is
// left out of the firmware. See "Reducing Firmware Size" in the README.
#define MENU_MINIMAL
#define MENU_ENABLE_TIME 1
#include "ESP32_MenuSystem.h"

// ── Display ───────────────────────────────────────────────────────────────────
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset */ U8X8_PIN_NONE);

// ── Button pins ───────────────────────────────────────────────────────────────
#define PIN_UP   12
#define PIN_DOWN 13
#define PIN_OK   14

// ── Time storage (all in 24-h internally) ────────────────────────────────────
int alarmH = 7,  alarmM = 30, alarmS = 0;   // alarm  – HH:mm:ss, 24-h
int wakeH  = 6,  wakeM  = 0;                 // wake   – HH:mm, 12-h mode
int timerS = 30;                              // timer  – seconds only
int shiftH = 8,  shiftM = 0;                 // shift  – HH:mm, restricted range + step

// ── Adjusters ─────────────────────────────────────────────────────────────────

// 1. Full HH:mm:ss, 24-hour (default)
TimeAdjuster taAlarm(&alarmH, &alarmM, &alarmS,
                     TIME_HOUR | TIME_MINUTE | TIME_SECOND,
                     TIME_MODE_24);

// 2. HH:mm with 12-hour mode (am/pm column added automatically)
TimeAdjuster taWake(&wakeH, &wakeM, nullptr,
                    TIME_HOUR | TIME_MINUTE,
                    TIME_MODE_12);

// 3. Seconds only
TimeAdjuster taTimer(nullptr, nullptr, &timerS,
                     TIME_SECOND,
                     TIME_MODE_24);

// 4. Restricted range + step  (shift starts at 04:00, hours 4-12, minutes 0/15/30/45)
TimeAdjuster taShift(&shiftH, &shiftM, nullptr,
                     TIME_HOUR | TIME_MINUTE,
                     TIME_MODE_24);

// ── MenuSystem ────────────────────────────────────────────────────────────────
MenuSystem menu(&display, PIN_UP, PIN_DOWN, PIN_OK);

void setup() {
    // Restrict shift adjuster fields
    taShift.setHourRange(4, 12, 1);
    taShift.setMinuteRange(0, 45, 15);

    menu.begin();

    int root = menu.addMenu("Time Settings");

    // Items appear with a right-side summary string (e.g. "07:30:00")
    menu.addTimeMenuItem(root, "Alarm",    &taAlarm);
    menu.addTimeMenuItem(root, "Wake",     &taWake);
    menu.addTimeMenuItem(root, "Timer (s)",&taTimer);
    menu.addTimeMenuItem(root, "Shift",    &taShift);
}

void loop() {
    menu.update();
}
