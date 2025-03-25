// ESP32_MenuSystem.h
#ifndef ESP32_MENU_SYSTEM_H
#define ESP32_MENU_SYSTEM_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <driver/gpio.h>

// Detect which ESP32 variant we're using
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32)
  #include <soc/soc_caps.h>
  #if SOC_PCNT_SUPPORTED
    #include <ESP32Encoder.h>
    #define USE_ESP32_ENCODER
  #else
    #include "InterruptEncoder.h"
    #define USE_INTERRUPT_ENCODER
  #endif
#else
  #include "InterruptEncoder.h"
  #define USE_INTERRUPT_ENCODER
#endif

#define MAX_MENU_ITEMS 16
#define MAX_MENU_NAME_LENGTH 16
#define MAX_MENU_DEPTH 32

#define ADJUSTER_TYPE_FLOAT 0
#define ADJUSTER_TYPE_INT 1
#define ADJUSTER_TYPE_BOOL 2

// Button trigger types
enum ButtonTriggerType {
    TRIGGER_LOW,    // Button active when LOW (connected to GND when pressed)
    TRIGGER_HIGH    // Button active when HIGH (connected to VCC when pressed)
};

// Button identifiers
enum ButtonID {
    BUTTON_ID_UP,
    BUTTON_ID_DOWN,
    BUTTON_ID_OK,
    BUTTON_ID_ENCODER
};

// Input mode options
enum InputMode {
    INPUT_BUTTONS,     // Traditional up/down/ok buttons
    INPUT_ENCODER      // Rotary encoder with button
};

// Forward declarations
class Menu;
class ValueAdjuster;

// Forward declare the callback type
typedef void (*ScreenInfoCallback)();
typedef void (*SimpleMenuFunction)();

// Menu callback base class
class MenuCallback {
public:
    virtual void execute() = 0;
};

class FunctionCallback : public MenuCallback {
    private:
        SimpleMenuFunction function;
        
    public:
        FunctionCallback(SimpleMenuFunction func) : function(func) {}
        
        void execute() override {
            if (function) {
                function();
            }
        }
    };

// Menu item class
class MenuItem {
    public:
        char name[MAX_MENU_NAME_LENGTH];
        int nextMenuId;
        MenuCallback* callback;
        ValueAdjuster* valueAdjuster;  // For items that adjust values
        
        MenuItem() : nextMenuId(-1), callback(nullptr), valueAdjuster(nullptr) {
            name[0] = '\0';
        }
        
        MenuItem(const char* itemName, int nextId = -1, MenuCallback* cb = nullptr, ValueAdjuster* adjuster = nullptr) 
            : nextMenuId(nextId), callback(nullptr), valueAdjuster(adjuster) {
            strncpy(name, itemName, MAX_MENU_NAME_LENGTH - 1);
            name[MAX_MENU_NAME_LENGTH - 1] = '\0';
            callback = cb;
        }
    };

// Menu class
class Menu {
public:
    char title[MAX_MENU_NAME_LENGTH];
    MenuItem items[MAX_MENU_ITEMS];
    int itemCount;
    int id;
    
    // Add screen info callback
    ScreenInfoCallback screenInfoCallback;
    bool hasScreenInfo;

    int maxVisibleItems;  // Maximum number of items to display at once (0 = auto/fit screen)
   
    Menu() : itemCount(0), id(-1), screenInfoCallback(nullptr), 
             hasScreenInfo(false), maxVisibleItems(0) {  // Default is 0 (auto)
        title[0] = '\0';
    }
    
    Menu(const char* menuTitle, int menuId) : itemCount(0), id(menuId),
             screenInfoCallback(nullptr), hasScreenInfo(false),
             maxVisibleItems(0) {  // Default is 0 (auto)
        strncpy(title, menuTitle, MAX_MENU_NAME_LENGTH - 1);
        title[MAX_MENU_NAME_LENGTH - 1] = '\0';
    }
    
    void addItem(const char* name, int nextMenuId = -1, MenuCallback* callback = nullptr, ValueAdjuster* adjuster = nullptr) {
        if (itemCount < MAX_MENU_ITEMS) {
            items[itemCount] = MenuItem(name, nextMenuId, callback, adjuster);
            itemCount++;
        }
    }

    void setMaxVisibleItems(int max) {
        maxVisibleItems = max;
    }
    
    // Add method to set screen info callback
    void setScreenInfoCallback(ScreenInfoCallback callback) {
        screenInfoCallback = callback;
        hasScreenInfo = (callback != nullptr);
    }
};

// Value adjuster interface classes remain unchanged...
class ValueAdjuster {
    public:
        // Type identifiers
        static const int TYPE_FLOAT = 0;
        static const int TYPE_INT = 1;
        static const int TYPE_BOOL = 2;
        
        virtual float getValue() = 0;
        virtual void setValue(float newValue) = 0;
        virtual float getIncrement() = 0;
        virtual float getMin() = 0;
        virtual float getMax() = 0;
        virtual const char* getUnit() = 0;
        virtual int getDecimalPlaces() = 0;
        
        // Add type identification method
        virtual int getType() const { return TYPE_FLOAT; } // Default type
    };
    
// Implementation of a float value adjuster
class FloatValueAdjuster : public ValueAdjuster {
    private:
    float* valuePtr;
    float increment;
    float minValue;
    float maxValue;
    int decimalPlaces;
    const char* unit;
    bool wrapAround; 
    
public:
    FloatValueAdjuster(float* value, float inc, float min, float max, 
                      int decimals = 1, const char* valueUnit = "", 
                      bool wrap = true) // Added wrap parameter
        : valuePtr(value), increment(inc), minValue(min), maxValue(max),
          decimalPlaces(decimals), unit(valueUnit), wrapAround(wrap) {}

    int getType() const override { return TYPE_FLOAT; }
    
    float getValue() override { return *valuePtr; }
    
    
    void setValue(float newValue) override {
        if (wrapAround) {
            // Apply limits with wrapping
            if (newValue > maxValue) {
                newValue = minValue;
            } else if (newValue < minValue) {
                newValue = maxValue;
            }
        } else {
            // Apply limits without wrapping (original behavior)
            if (newValue < minValue) newValue = minValue;
            if (newValue > maxValue) newValue = maxValue;
        }
        *valuePtr = newValue;
    }
    
    float getIncrement() override { return increment; }
    float getMin() override { return minValue; }
    float getMax() override { return maxValue; }
    const char* getUnit() override { return unit; }
    int getDecimalPlaces() override { return decimalPlaces; }
};

// Implementation of an integer value adjuster
class IntValueAdjuster : public ValueAdjuster {
    private:
        int* valuePtr;
        int increment;
        int minValue;
        int maxValue;
        const char* unit;
        bool wrapAround;
        
    public:
    IntValueAdjuster(int* value, int inc, int min, int max, 
        const char* valueUnit = "", bool wrap = true) 
        : valuePtr(value), increment(inc), minValue(min), maxValue(max),
            unit(valueUnit), wrapAround(wrap) {}

        int getType() const override { return TYPE_INT; }
        
        float getValue() override { return static_cast<float>(*valuePtr); }
        
        void setValue(float newValue) override {
            // Convert to int
            int intValue = static_cast<int>(newValue);
            
            if (wrapAround) {
                // Apply limits with wrapping
                if (intValue > maxValue) {
                    intValue = minValue; // Wrap to minimum value
                } else if (intValue < minValue) {
                    intValue = maxValue; // Wrap to maximum value 
                }
            } else {
                // Apply limits without wrapping (original behavior)
                if (intValue < minValue) intValue = minValue;
                if (intValue > maxValue) intValue = maxValue;
            }
            
            *valuePtr = intValue;
        }
        float getIncrement() override { return static_cast<float>(increment); }
        float getMin() override { return static_cast<float>(minValue); }
        float getMax() override { return static_cast<float>(maxValue); }
        const char* getUnit() override { return unit; }
        int getDecimalPlaces() override { return 0; } // Always 0 for integers
    };

// Implementation of a boolean value adjuster with custom text labels
class BoolValueAdjuster : public ValueAdjuster {
    private:
        bool* valuePtr;         // Points to the actual stored boolean value
        bool tempValue;         // Temporary value for selection
        char trueLabel[MAX_MENU_NAME_LENGTH];
        char falseLabel[MAX_MENU_NAME_LENGTH];
        char description[MAX_MENU_NAME_LENGTH];
        int adjusterType;       // Simple type identifier
        
    public:
        BoolValueAdjuster(bool* value, const char* trueText = "On", const char* falseText = "Off", 
                         const char* desc = "") : valuePtr(value), tempValue(*value) {
            // Set type explicitly
            adjusterType = ADJUSTER_TYPE_BOOL;
            
            // Copy labels with length protection
            strncpy(trueLabel, trueText, MAX_MENU_NAME_LENGTH - 1);
            trueLabel[MAX_MENU_NAME_LENGTH - 1] = '\0';
            
            strncpy(falseLabel, falseText, MAX_MENU_NAME_LENGTH - 1);
            falseLabel[MAX_MENU_NAME_LENGTH - 1] = '\0';
            
            strncpy(description, desc, MAX_MENU_NAME_LENGTH - 1);
            description[MAX_MENU_NAME_LENGTH - 1] = '\0';
        }
        
        float getValue() override { 
            return *valuePtr ? 1.0f : 0.0f; 
        }
        
        void setValue(float newValue) override {
            // For boolean, just toggle the value regardless of the input
            // This ensures it always cycles between true and false
            *valuePtr = !(*valuePtr);
        }
        
        float getIncrement() override { return 1.0f; }
        float getMin() override { return 0.0f; }
        float getMax() override { return 1.0f; }
        const char* getUnit() override { return ""; }
        int getDecimalPlaces() override { return 0; }
        
        // Type identification - override the getType method from ValueAdjuster
        int getType() const override { return adjusterType; }
        
        // Specific methods for BoolValueAdjuster
        const char* getTrueLabel() const { return trueLabel; }
        const char* getFalseLabel() const { return falseLabel; }
        const char* getDescription() const { return description; }
        const char* getCurrentLabel() const { return *valuePtr ? trueLabel : falseLabel; }
        
        bool isTrue() const { return *valuePtr; }
        
        // Methods for temporary selection
        void setTempValue(bool value) { tempValue = value; }
        bool getTempValue() const { return tempValue; }
        void applyTempValue() { *valuePtr = tempValue; }
        const char* getTempLabel() const { return tempValue ? trueLabel : falseLabel; }
    };

// Main menu system class with combined input support
class ESP32_MenuSystem {
private:
    Menu menus[MAX_MENU_DEPTH];
    int menuCount;
    int currentMenuIndex;
    int cursorPosition;

    // Screen properties
    uint16_t screenWidth;  // Screen width in pixels
    uint16_t screenHeight; // Screen height in pixels

    // Display offset properties
    int16_t displayOffsetX;
    int16_t displayOffsetY;
    bool useDisplayOffset;

    // Layout variables (computed based on screen size)
    uint8_t titleHeight;          // Height of title bar
    uint8_t separatorY;           // Y position of separator line
    uint8_t menuStartY;           // Starting Y position for menu items
    uint8_t lineHeight;           // Distance between menu items
    uint8_t menuItemsVisible;     // Number of menu items visible at once
    uint8_t scrollIndicatorWidth; // Width of scroll indicator if needed
    uint8_t menuItemPadding;
    
    // Font 
    const uint8_t* standardFont;   // Default: u8g2_font_5x8_tr
    const uint8_t* titleFont;      // Default: u8g2_font_6x12_tr
    const uint8_t* valueFont;      // Default: u8g2_font_10x20_tr
    uint8_t getFontHeight(const uint8_t* font);
    uint8_t getFontWidth(const uint8_t* font);
    void updateLayoutForFonts();
    U8G2* display;
    
    // Input mode
    InputMode inputMode;
    
    // Button pins and states (for INPUT_BUTTONS mode)
    int buttonUpPin;
    int buttonDownPin;
    int buttonOkPin;
    bool buttonUpState;
    bool buttonDownState;
    bool buttonOkState;
    bool lastButtonUpState;
    bool lastButtonDownState;
    bool lastButtonOkState;
    
    // Button trigger types
    ButtonTriggerType buttonUpTriggerType;
    ButtonTriggerType buttonDownTriggerType;
    ButtonTriggerType buttonOkTriggerType;
    ButtonTriggerType encoderButtonTriggerType;
    
    // Rotary encoder variables (for INPUT_ENCODER mode)
    #ifdef USE_ESP32_ENCODER
    ESP32Encoder* encoder;
    #else
    InterruptEncoder* encoder;
    #endif
    int encoderPinA;
    int encoderPinB;
    int encoderButtonPin;
    long lastEncoderValue;
    bool encoderButtonState;
    bool lastEncoderButtonState;
    int encoderSensitivity;   // Number of encoder steps to register as one menu movement
    long encoderAccumulator;  // Accumulator for encoder ticks
    
    // Value adjustment mode
    bool isValueAdjustMode;
    ValueAdjuster* currentValueAdjuster;
    
    // Debouncing
    unsigned long lastDebounceTime;
    unsigned long debounceDelay;
    
    // For timed operations
    unsigned long previousMillis;
    unsigned long interval;
    
    // Error handling
    int errorCode;
    char errorMessage[64];
    
public:
    // Constructor for button mode
    ESP32_MenuSystem(U8G2* u8g2Display, int upPin, int downPin, int okPin);
    
    // Constructor for encoder mode
    ESP32_MenuSystem(U8G2* u8g2Display, int encoderA, int encoderB, int encoderBtn, bool useEncoder, int sensitivity = 1);
    
    // Destructor
    ~ESP32_MenuSystem();

    void begin();
    
    // Menu management
    int addMenu(const char* title);
    void addMenuItem(int menuIndex, const char* name, int nextMenuId = -1, MenuCallback* callback = nullptr);
    void addValueMenuItem(int menuIndex, const char* name, ValueAdjuster* adjuster);
    void setMenuMaxVisibleItems(int menuIndex, int maxItems);

    // Screen size customization
    void setScreenSize(uint16_t width, uint16_t height);
    uint16_t getScreenWidth() const { return screenWidth; }
    uint16_t getScreenHeight() const { return screenHeight; }
    // Set the padding between menu items
    void setMenuItemPadding(uint8_t padding) { 
    menuItemPadding = padding; 
    updateLayoutForFonts(); } // Recalculate layout with new padding

    // Display offset configuration
    void setDisplayOffset(int16_t x, int16_t y);
    void clearDisplayOffset();
    bool isUsingDisplayOffset() const { return useDisplayOffset; }
    int16_t getDisplayOffsetX() const { return displayOffsetX; }
    int16_t getDisplayOffsetY() const { return displayOffsetY; }

    // For advanced customization
    void setLayoutParameters(uint8_t titleH, uint8_t sepY, uint8_t startY, uint8_t lineH);

    // Button trigger configuration
    void configureButtonTriggers(ButtonTriggerType upTrigger = TRIGGER_LOW,
                                ButtonTriggerType downTrigger = TRIGGER_LOW,
                                ButtonTriggerType okTrigger = TRIGGER_LOW,
                                ButtonTriggerType encoderTrigger = TRIGGER_LOW);
                                
    void setButtonTrigger(ButtonID buttonId, ButtonTriggerType triggerType);

    // Font
    void setStandardFont(const uint8_t* font) { standardFont = font; }
    void setTitleFont(const uint8_t* font) { titleFont = font; }
    void setValueFont(const uint8_t* font) { valueFont = font; }
    
    const uint8_t* getStandardFont() const { return standardFont; }
    const uint8_t* getTitleFont() const { return titleFont; }
    const uint8_t* getValueFont() const { return valueFont; }
    
    void setFonts(const uint8_t* standard, const uint8_t* title, const uint8_t* value) {
        standardFont = standard;
        titleFont = title;
        valueFont = value;
    }
    
    // Navigation
    void moveUp();
    void moveDown();
    void select();
    void goBack();
    void setCurrentMenu(int menuId);
    int getCursorPosition() const { return cursorPosition; }
    int getCurrentMenuId() const;
    
    // Input handling
    void checkButtons();
    void handleEncoderMovement();
    void handleButtonPress();
    
    // Value adjustment
    void enterValueAdjustMode(ValueAdjuster* adjuster);
    void exitValueAdjustMode();
    
    // Display
    void displayMenu();
    void displayValueAdjust();
    void displayBoolAdjust();
    void displayError();
    void addScreenInfo(int menuIndex, ScreenInfoCallback callback);
    
    // Error handling
    void setError(int code, const char* message);
    void clearError();
    
    // Main update function to call in loop()
    void update();
    // Add this to the public section of ESP32_MenuSystem class
    void addMenuItemWithFunction(int menuIndex, const char* name, SimpleMenuFunction function, int nextMenuId = -1) {
        if (menuIndex >= 0 && menuIndex < menuCount) {
            // Create a new FunctionCallback (will be managed by the library)
            FunctionCallback* callback = new FunctionCallback(function);
        
            // Add the menu item with this callback
            menus[menuIndex].addItem(name, nextMenuId, callback);
        }
    }

    typedef void (*ScreenInfoCallback)();
    
    // Helper functions
    int findMenuById(int id);
    Menu* getCurrentMenu();
};

#endif // ESP32_MENU_SYSTEM_H
