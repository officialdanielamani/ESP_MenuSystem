// SensorDashboard.ino  –  ESP32_MenuSystem example
// Demonstrates DisplayGaugeInt and DisplayGaugeFloat fill-style gauges.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <U8g2lib.h>
#include "ESP32_MenuSystem.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define BTN_UP   12
#define BTN_DOWN 13
#define BTN_OK   14

// ── Live sensor readings ──────────────────────────────────────────────────────
int   roomTemp = 25;       // C      0-50     -> DisplayGaugeInt
float voltage  = 3.31f;   // V      0-5      -> DisplayGaugeFloat (2 decimals)
float pressure = 101.3f;  // hPa    800-1200 -> DisplayGaugeFloat (1 decimal)

// ── User-editable settings ────────────────────────────────────────────────────
float targetTemp = 24.0f;
int   fanSpeed   = 60;

// ── Gauges ────────────────────────────────────────────────────────────────────
//  DisplayGaugeInt  (ptr, min, max, unit, sweepDeg)
//  DisplayGaugeFloat(ptr, min, max, unit, sweepDeg, decimals)
//
//  decimals=0  -> displayed as integer: "25"
//  decimals=1  -> one decimal place:    "3.3"
//  decimals=2  -> two decimal places:   "3.31"

DisplayGaugeInt   gTemp    (&roomTemp, 0,    50,   "C", 270);
DisplayGaugeFloat gVoltage (&voltage,  0.0f, 5.0f, "V", 270, 2);
DisplayGaugeFloat gPressure(&pressure, 800,  1200, "hPa", 180, 1);

// ── Editable adjusters ────────────────────────────────────────────────────────
FloatValueAdjuster targetAdj(&targetTemp, 0.5f, 16.0f, 32.0f, 1, "C");
IntValueAdjuster   fanAdj   (&fanSpeed,   5,    0,     100,   "%");

MenuSystem menu(&u8g2, BTN_UP, BTN_DOWN, BTN_OK);

void setup() {
    u8g2.begin();
    menu.setFontPreset(FONT_PRESET_NORMAL);
    menu.begin();

    int dash = menu.addMenu("Dashboard");
    menu.addDisplayGaugeInt  (dash, "Room Temp",  &gTemp);
    menu.addDisplayGaugeFloat(dash, "Voltage",    &gVoltage);
    menu.addDisplayGaugeFloat(dash, "Pressure",   &gPressure);
    menu.addValueMenuItem    (dash, "Target Temp", &targetAdj);
    menu.addValueMenuItem    (dash, "Fan Speed",   &fanAdj);
}

void simulateSensors() {
    static unsigned long last = 0;
    if (millis() - last < 1000) return;
    last = millis();
    roomTemp = constrain(roomTemp + random(-1, 2), 0, 50);
    voltage  = constrain(voltage  + (random(-10, 11) * 0.01f), 0.0f, 5.0f);
    pressure = constrain(pressure + (random(-5,   6) * 0.1f),  800.0f, 1200.0f);
}

void loop() {
    simulateSensors();
    menu.update();
}
