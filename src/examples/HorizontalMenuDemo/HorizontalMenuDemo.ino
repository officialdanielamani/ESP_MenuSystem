// HorizontalMenuDemo.ino  –  ESP32_MenuSystem example
// Demonstrates the setMenuLayout() API for controlling visible items and anchoring.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <U8g2lib.h>
#include "ESP32_MenuSystem.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define BTN_UP   12
#define BTN_DOWN 13
#define BTN_OK   14

MenuSystem menu(&u8g2, BTN_UP, BTN_DOWN, BTN_OK);

// ── Value adjusters ────────────────────────────────────────────────────────
// Adjusters edit the variable they point to: (&value, step, min, max, ...)
float targetTemp = 22.0f;
int   fanSpeed   = 50;
bool  heaterOn   = false;
FloatValueAdjuster tempAdj(&targetTemp, 0.5f, 10.0f, 35.0f, 1, "C");
IntValueAdjuster   fanAdj(&fanSpeed, 5, 0, 100, "%");
BoolValueAdjuster  heaterAdj(&heaterOn, "ON", "OFF");

// ── Sensor for gauge ───────────────────────────────────────────────────────
int humidity = 55;
DisplayGaugeInt gHumidity(&humidity, 0, 100, "%", 270);

int mainMenuId, settingsMenuId, sensorMenuId;

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    menu.setFontPreset(FONT_PRESET_NORMAL);
    menu.begin();

    // ── Main menu: no setMenuLayout → uses display default ──────────────────
    mainMenuId     = menu.addMenu("Main Menu");
    settingsMenuId = menu.addMenu("Quick Set");
    sensorMenuId   = menu.addMenu("Sensors");

    menu.addMenuItem(mainMenuId, "Quick Set",  settingsMenuId);
    menu.addMenuItem(mainMenuId, "Sensors",    sensorMenuId);

    // ── Settings: 8 char name limit, 1 item visible, anchored at TOP ─────
    //  Names longer than 8 chars are truncated. When selected, they
    //  auto-scroll to reveal the full name, then loop back.
    //
    //    Quick Set                1/3
    //    ─────────────────────────────
    //    > Temperat...       22.0C      ← truncated
    //       (scrolls to "ature " when selected)
    //
    menu.setMenuLayout(settingsMenuId, 8, 1);     // 8 chars, 1 item, TOP, counter on
    menu.addValueMenuItem(settingsMenuId, "Temperature Home", &tempAdj);
    menu.addValueMenuItem(settingsMenuId, "Fan Speed Fast",   &fanAdj);
    menu.addValueMenuItem(settingsMenuId, "Heater",           &heaterAdj);

    // ── Sensors: no char limit, 1 item visible, anchored at BOTTOM ──────────
    menu.setMenuLayout(sensorMenuId, 0, 1, BOTTOM, false);
    menu.addDisplayGaugeInt(sensorMenuId, "Humidity", &gHumidity);

    menu.configureButtonTriggers(TRIGGER_LOW, TRIGGER_LOW, TRIGGER_LOW);
}

void simulateSensors() {
    static unsigned long last = 0;
    if (millis() - last < 2000) return;
    last = millis();
    humidity = constrain(humidity + random(-3, 4), 0, 100);
}

void loop() {
    simulateSensors();
    menu.update();
}
