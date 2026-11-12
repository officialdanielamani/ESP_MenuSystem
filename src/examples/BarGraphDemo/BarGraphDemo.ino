// BarGraphDemo.ino  –  ESP32_MenuSystem example
// Demonstrates DisplayBarInt and DisplayBarFloat bar graph widgets.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <U8g2lib.h>

// This sketch only uses bar graphs, so every other optional widget is
// left out of the firmware. See "Reducing Firmware Size" in the README.
#define MENU_MINIMAL
#define MENU_ENABLE_BAR 1
#include "ESP32_MenuSystem.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define BTN_UP   12
#define BTN_DOWN 13
#define BTN_OK   14

// ── Live sensor readings ──────────────────────────────────────────────────────
int   roomTemp  = 25;       // C      -20..50
float humidity  = 65.3f;    // %      0..100
int   powerW    = 18;       // W      0..30

// ── Bar widgets ───────────────────────────────────────────────────────────────

// Default (full-screen) bar — big centred value + bar + min/max
DisplayBarInt   bTemp(&roomTemp, -20, 50, "C");

// Default bar with useOver = true — shows "25/50 C"
DisplayBarInt   bPower(&powerW, 0, 30, "W", BAR_DEFAULT, true, true);

// Compact bar — two-row: name+value, then bar+min/max
DisplayBarFloat bHumidity(&humidity, 0.0f, 100.0f, "%", 1, BAR_COMPACT, true, false);

// Compact bar, no min/max labels — bar extends full width
DisplayBarInt   bTempCompact(&roomTemp, -20, 50, "C", BAR_COMPACT, false, false);

// Compact bar with useOver — shows "18/30W" on the right
DisplayBarInt   bPowerCompact(&powerW, 0, 30, "W", BAR_COMPACT, false, true);

MenuSystem menu(&u8g2, BTN_UP, BTN_DOWN, BTN_OK);

int mainId, defaultBarsId, compactBarsId;

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    menu.setFontPreset(FONT_PRESET_NORMAL);
    menu.begin();

    // ── Main menu ─────────────────────────────────────────────────────────────
    mainId         = menu.addMenu("Bar Demo");
    defaultBarsId  = menu.addMenu("Full Screen");
    compactBarsId  = menu.addMenu("Compact");

    menu.addMenuItem(mainId, "Full Screen", defaultBarsId);
    menu.addMenuItem(mainId, "Compact",     compactBarsId);

    // ── Full-screen bars: auto-open like gauges ───────────────────────────────
    //  Scrolling through these shows a full-screen bar per item.
    menu.addDisplayBarInt  (defaultBarsId, "Temperature", &bTemp);
    menu.addDisplayBarInt  (defaultBarsId, "Power",       &bPower);

    // ── Compact bars: shown inline in the menu list ───────────────────────────
    //  Each bar takes two rows in the list: name+value, then the bar itself.
    menu.addDisplayBarFloat(compactBarsId, "Humidity",    &bHumidity);
    menu.addDisplayBarInt  (compactBarsId, "Temperature", &bTempCompact);
    menu.addDisplayBarInt  (compactBarsId, "Power",       &bPowerCompact);

    menu.configureButtonTriggers(TRIGGER_LOW, TRIGGER_LOW, TRIGGER_LOW);
}

void simulateSensors() {
    static unsigned long last = 0;
    if (millis() - last < 1000) return;
    last = millis();
    roomTemp = constrain(roomTemp + random(-2, 3), -20, 50);
    humidity = constrain(humidity + (random(-10, 11) * 0.1f), 0.0f, 100.0f);
    powerW   = constrain(powerW  + random(-2, 3), 0, 30);
}

void loop() {
    simulateSensors();
    menu.update();
}
