//Testing Library Functions — with Dynamic MultiSelect demo
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32_MenuSystem.h>

// -----------------------------------------------------------------------------
// Pin definitions
// -----------------------------------------------------------------------------
#define ENCODER_A_PIN   35
#define ENCODER_B_PIN   33
#define ENCODER_BTN_PIN 13
#define I2C_SDA         21
#define I2C_SCL         22

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------
static U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
    U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);

// -----------------------------------------------------------------------------
// Menu system (rotary encoder mode, sensitivity = 2)
// -----------------------------------------------------------------------------
static ESP32_MenuSystem menu(&display,
                             ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_BTN_PIN,
                             true, 2);

// -----------------------------------------------------------------------------
// Mock sensor readings
// -----------------------------------------------------------------------------
static float mockTemperature = 24.3f;
static int   mockHumidity    = 62;

// -----------------------------------------------------------------------------
// Live setting variables
// -----------------------------------------------------------------------------
static float targetTemp    = 22.0f;
static bool  acEnabled     = false;
static int   humidityLimit = 60;

// -----------------------------------------------------------------------------
// Value adjusters
// -----------------------------------------------------------------------------
static FloatValueAdjuster tempAdj(
    &targetTemp, 0.5f, 18.0f, 30.0f, 1, "C", false);

static BoolValueAdjuster acAdj(
    &acEnabled, "On", "Off");

static IntValueAdjuster humidAdj(
    &humidityLimit, 5, 30, 90, "%", false);

// ── Static multi-select: days of week (old-style addOption) ───────────────────
static MultiSelectAdjuster activeDays;

// ── Dynamic multi-select: random scan simulation ──────────────────────────────
// 10 slots, select min 1 max 3, hide empty slots.
// After "scanning", only the populated slots appear in the list.
static MultiSelectAdjuster randomScan(10, 1, 3, true);

// Pool of fake item names the random scan picks from
static const char* scanPool[] = {
    "Alpha",   "Bravo",   "Charlie", "Delta",  "Echo",
    "Foxtrot", "Golf",    "Hotel",   "India",  "Juliet",
    "Kilo",    "Lima",    "Mike",    "Nov",    "Oscar"
};
static const uint8_t POOL_SIZE = sizeof(scanPool) / sizeof(scanPool[0]);

// ── Dynamic multi-select: WiFi-style (max 1 selection) ───────────────────────
// 10 slots, select exactly 0 or 1, hide empty.
static MultiSelectAdjuster wifiPick(10, 0, 1, true);

static const char* wifiPool[] = {
    "Home_WiFi",    "Office_5G",    "CoffeeShop",  "Neighbor_AP",
    "IoT_Network",  "Guest_WiFi",   "Lab_Secure",  "Mesh_Node3",
    "OpenWRT_24G",  "StarLink_Out", "Mobile_Hot",   "Hidden_Net"
};
static const uint8_t WIFI_POOL_SIZE = sizeof(wifiPool) / sizeof(wifiPool[0]);

// =============================================================================
// Date / Time / Gauge / Bar / String adjusters
// =============================================================================
int day = 21, month = 8, year = 2021;
static DateAdjuster myDate(
    nullptr, &month, &year,
    DATE_MONTH | DATE_YEAR,
    DATE_ORDER_YMD,
    DATE_MONTH_LONG,
    2000, 2099
);

int hour = 8, minute_val = 0, second_val = 0;
static TimeAdjuster myTime(
    &hour, &minute_val, nullptr,
    TIME_HOUR | TIME_MINUTE,
    TIME_MODE_12
);

int roomTemp = 27;
DisplayGaugeInt    gaugeTemp(&roomTemp, 0, 50, "C", 180, false);
DisplayGaugeFloat  gaugeReal(&mockTemperature, 0, 50, "C", 180, 1);
DisplayBarFloat    barCompact(&mockTemperature, -10, 40, "C", 1, BAR_COMPACT, false, true);

char ip[33] = "192.168.1.1";
StringAdjuster myIp(ip, STRING_PALETTE_DIGITS ".", 7, 15);

// -----------------------------------------------------------------------------
// Menu IDs
// -----------------------------------------------------------------------------
static int mainMenu     = -1;
static int settingsMenu = -1;
static int displayMenu  = -1;

// =============================================================================
// RANDOM SCAN CALLBACK
// Simulates discovering 0–9 items from the pool, populates randomScan.
// =============================================================================
static void onRandomScan() {
    randomScan.clearOptions();

    int found = random(0, 10);   // 0 to 9 items

    Serial.print("[Scan] Found ");
    Serial.print(found);
    Serial.println(" items:");

    if (found == 0) {
        Serial.println("  (none)");
        return;   // list stays empty → widget will show [None]
    }

    // Shuffle-pick `found` unique items from the pool
    // Simple Fisher-Yates partial shuffle on an index array
    uint8_t indices[POOL_SIZE];
    for (uint8_t i = 0; i < POOL_SIZE; i++) indices[i] = i;
    for (uint8_t i = 0; i < found && i < POOL_SIZE; i++) {
        uint8_t j = random(i, POOL_SIZE);
        uint8_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    for (int i = 0; i < found; i++) {
        randomScan.setOption(i, scanPool[indices[i]]);
        Serial.print("  - ");
        Serial.println(scanPool[indices[i]]);
    }
}

// =============================================================================
// WIFI SCAN CALLBACK
// Same idea but for single-select WiFi style (0 or 1 pick allowed).
// =============================================================================
static void onWifiScan() {
    wifiPick.clearOptions();

    int found = random(0, 8);   // 0 to 7 networks

    Serial.print("[WiFi] Found ");
    Serial.print(found);
    Serial.println(" networks:");

    if (found == 0) {
        Serial.println("  (none)");
        return;
    }

    uint8_t indices[WIFI_POOL_SIZE];
    for (uint8_t i = 0; i < WIFI_POOL_SIZE; i++) indices[i] = i;
    for (uint8_t i = 0; i < found && i < WIFI_POOL_SIZE; i++) {
        uint8_t j = random(i, WIFI_POOL_SIZE);
        uint8_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    for (int i = 0; i < found; i++) {
        wifiPick.setOption(i, wifiPool[indices[i]]);
        Serial.print("  - ");
        Serial.println(wifiPool[indices[i]]);
    }
}

// =============================================================================
// PRINT RESULTS CALLBACK
// Shows what the user selected in each multi-select.
// =============================================================================
static void onShowResults() {
    Serial.println("=== Multi-Select Results ===");

    // Static days
    Serial.print("  Days: ");
    bool first = true;
    for (uint8_t i = 0; i < 7; i++) {
        if (activeDays.isSelected(i)) {
            if (!first) Serial.print(", ");
            Serial.print(activeDays.getLabel(i));
            first = false;
        }
    }
    if (first) Serial.print("(none)");
    Serial.println();

    // Dynamic random scan
    Serial.print("  Random: ");
    first = true;
    for (uint8_t i = 0; i < randomScan.getCapacity(); i++) {
        if (randomScan.isSelected(i)) {
            if (!first) Serial.print(", ");
            Serial.print(randomScan.getLabel(i));
            first = false;
        }
    }
    if (first) Serial.print("(none)");
    Serial.println();

    // Dynamic WiFi
    Serial.print("  WiFi: ");
    first = true;
    for (uint8_t i = 0; i < wifiPick.getCapacity(); i++) {
        if (wifiPick.isSelected(i)) {
            if (!first) Serial.print(", ");
            Serial.print(wifiPick.getLabel(i));
            first = false;
        }
    }
    if (first) Serial.print("(none)");
    Serial.println();
}

// =============================================================================
// DISPLAY STYLE CALLBACKS
// =============================================================================
void onFontSmall()  { menu.setFontPreset(FONT_PRESET_SMALL);  }
void onFontNormal() { menu.setFontPreset(FONT_PRESET_NORMAL); }
void onFontLarge()  { menu.setFontPreset(FONT_PRESET_LARGE);  }

void onFontHelvetica() {
    menu.setFonts(u8g2_font_helvR08_tr,
                  u8g2_font_helvB08_tr,
                  u8g2_font_helvB08_tr);
}
void onFontProfont() {
    menu.setFonts(u8g2_font_profont10_tr,
                  u8g2_font_profont12_tr,
                  u8g2_font_profont22_tr);
}
void onFontMicro() {
    menu.setFonts(u8g2_font_micro_tr,
                  u8g2_font_4x6_tr,
                  u8g2_font_6x12_tr);
}
void onBoldTitle() {
    menu.setTitleFont(u8g2_font_helvB12_tr);
}

void onSelArrow()   { menu.setSelectionStyle(SELECTION_ARROW);   }
void onSelInverse() { menu.setSelectionStyle(SELECTION_INVERSE); }
void onSelNone()    { menu.setSelectionStyle(SELECTION_NONE);    }
void onSelBlink()   { menu.setSelectionStyle(SELECTION_BLINK);   }
void onSelBracket() { menu.setSelectionStyle(SELECTION_BRACKET); }

void onToggleSeparator() { menu.setShowSeparator(!menu.getShowSeparator()); }
void onToggleScrollBar() { menu.setShowScrollBar(!menu.getShowScrollBar()); }

// =============================================================================
// SENSOR OVERLAY
// =============================================================================
static void drawSensorOverlay() {
    display.setFont(u8g2_font_5x8_tr);

    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "T:%.1fC", mockTemperature);
    display.setCursor(128 - (int)strlen(tempStr) * 6, 40);
    display.print(tempStr);

    char humStr[16];
    snprintf(humStr, sizeof(humStr), "H:%d%%", mockHumidity);
    display.setCursor(128 - (int)strlen(humStr) * 6, 52);
    display.print(humStr);
}

// =============================================================================
// APPLY CALLBACK
// =============================================================================
static void onApply() {
    Serial.println("=== Settings Applied ===");
    Serial.print("  Target temp  : "); Serial.print(targetTemp, 1); Serial.println(" C");
    Serial.print("  AC enabled   : "); Serial.println(acEnabled ? "On" : "Off");
    Serial.print("  Humidity lim : "); Serial.print(humidityLimit); Serial.println(" %");
    onShowResults();
}

// =============================================================================
// MOCK SENSOR SIMULATION
// =============================================================================
static unsigned long lastSensorUpdate = 0;

static void simulateSensors() {
    if (millis() - lastSensorUpdate < 3000UL) return;
    lastSensorUpdate = millis();

    mockTemperature += static_cast<float>(random(-1, 2)) * 0.1f;
    mockTemperature  = constrain(mockTemperature, 15.0f, 35.0f);

    mockHumidity += random(-1, 2);
    mockHumidity  = constrain(mockHumidity, 20, 95);
}

// =============================================================================
// MENU TREE
// =============================================================================
static void buildMenuTree() {
    mainMenu     = menu.addMenu("Climate Ctrl");
    settingsMenu = menu.addMenu("Settings");
    displayMenu  = menu.addMenu("Display");

    // ── Main menu ─────────────────────────────────────────────────────────────
    menu.addMenuItem           (mainMenu, "Settings", settingsMenu);
    menu.addMenuItemWithFunction(mainMenu, "Apply",   onApply);
    menu.addMenuItemWithFunction(mainMenu, "Results", onShowResults);
    menu.addScreenInfo         (mainMenu, drawSensorOverlay);

    // ── Settings menu ─────────────────────────────────────────────────────────
    menu.addValueMenuItem      (settingsMenu, "Target Temp",   &tempAdj);
    menu.addValueMenuItem      (settingsMenu, "AC Power",      &acAdj);
    menu.addValueMenuItem      (settingsMenu, "Humidity Limit", &humidAdj);
    menu.addDisplayGaugeInt    (settingsMenu, "Room Temp",     &gaugeTemp);
    menu.addDisplayGaugeFloat  (settingsMenu, "Real Temp",     &gaugeReal);
    menu.addDisplayBarFloat    (settingsMenu, "Temperature",   &barCompact);

    // Static multi-select: days of week
    menu.addMultiSelectMenuItem(settingsMenu, "Active Days",   &activeDays);

    // Dynamic multi-select: random scan (min 1, max 3)
    // User presses "Scan Random" to populate, then opens "Pick Random" to choose.
    menu.addMenuItemWithFunction(settingsMenu, "Scan Random",  onRandomScan);
    menu.addMultiSelectMenuItem(settingsMenu, "Pick Random",   &randomScan);

    // Dynamic multi-select: WiFi style (max 1)
    // User presses "Scan WiFi" to populate, then opens "Pick WiFi" to choose.
    menu.addMenuItemWithFunction(settingsMenu, "Scan WiFi",    onWifiScan);
    menu.addMultiSelectMenuItem(settingsMenu, "Pick WiFi",     &wifiPick);

    menu.addMenuItem           (settingsMenu, "Display",       displayMenu);
    menu.addDateMenuItem       (settingsMenu, "Date",          &myDate);
    menu.addTimeMenuItem       (settingsMenu, "Time",          &myTime);
    menu.addStringMenuItem     (settingsMenu, "IP Address",    &myIp);
    menu.addMenuItem           (settingsMenu, "< Back",        mainMenu);

    // ── Display style menu ────────────────────────────────────────────────────
    menu.addMenuItemWithFunction(displayMenu, "Font Small",    onFontSmall);
    menu.addMenuItemWithFunction(displayMenu, "Font Normal",   onFontNormal);
    menu.addMenuItemWithFunction(displayMenu, "Font Large",    onFontLarge);
    menu.addMenuItemWithFunction(displayMenu, "Helvetica",     onFontHelvetica);
    menu.addMenuItemWithFunction(displayMenu, "ProFont",       onFontProfont);
    menu.addMenuItemWithFunction(displayMenu, "Micro",         onFontMicro);
    menu.addMenuItemWithFunction(displayMenu, "Bold Title",    onBoldTitle);
    menu.addMenuItemWithFunction(displayMenu, "Sel: Arrow",    onSelArrow);
    menu.addMenuItemWithFunction(displayMenu, "Sel: Inverse",  onSelInverse);
    menu.addMenuItemWithFunction(displayMenu, "Sel: None",     onSelNone);
    menu.addMenuItemWithFunction(displayMenu, "Sel: Blink",    onSelBlink);
    menu.addMenuItemWithFunction(displayMenu, "Sel: Bracket",  onSelBracket);
    menu.addMenuItemWithFunction(displayMenu, "Separator",     onToggleSeparator);
    menu.addMenuItemWithFunction(displayMenu, "ScrollBar",     onToggleScrollBar);
    menu.addMenuItem            (displayMenu, "< Back",        settingsMenu);
}

// =============================================================================
// SETUP / LOOP
// =============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("ClimateController booting...");

    randomSeed(analogRead(0));
    display.begin();

    // Static days: Mon–Fri pre-selected (using legacy addOption)
    activeDays.addOption("Monday",    true);
    activeDays.addOption("Tuesday",   true);
    activeDays.addOption("Wednesday", true);
    activeDays.addOption("Thursday",  true);
    activeDays.addOption("Friday",    true);
    activeDays.addOption("Saturday",  false);
    activeDays.addOption("Sunday",    false);

    // Dynamic lists start empty — user triggers scan to populate

    menu.setFontPreset(FONT_PRESET_NORMAL);
    menu.begin();
    buildMenuTree();

    Serial.println("Ready. Rotate to navigate, press to select.");
    Serial.println("  'Scan Random' → populates 0-9 random items (pick 1-3)");
    Serial.println("  'Scan WiFi'   → populates 0-7 random networks (pick 0-1)");
    Serial.println("  'Pick Random' / 'Pick WiFi' → open the selector");
    Serial.println("  'Results'     → print all selections to Serial");
}

void loop() {
    simulateSensors();
    menu.update();
}
