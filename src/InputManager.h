// InputManager.h
// Button, encoder, and single-button input managers.
// Depends only on Arduino GPIO and a forward-declared MenuSystem interface.
// No display, no adjuster knowledge — calls only moveUp/moveDown/select/clearError.
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "MenuConfig.h"

// The encoder driver is only pulled in when encoder input is enabled, so
// button-only projects do not need the ESP32Encoder library installed.
#if MENU_ENABLE_ENCODER
  #if defined(ESP32)
    #include <ESP32Encoder.h>
    #define USE_ESP32_ENCODER
  #else
    #include "InterruptEncoder.h"
    #define USE_INTERRUPT_ENCODER
  #endif
#endif

// -----------------------------------------------------------------------------
// Trigger polarity
// -----------------------------------------------------------------------------
enum ButtonTriggerType {
    TRIGGER_LOW,    // active-low (default, INPUT_PULLUP)
    TRIGGER_HIGH    // active-high
};

enum InputMode {
    INPUT_BUTTONS,
    INPUT_ENCODER,
    INPUT_ONE_BUTTON
};

// -----------------------------------------------------------------------------
// IMenuController — minimal interface the input managers need from MenuSystem.
// Keeps InputManager decoupled from the full MenuSystem class definition.
// -----------------------------------------------------------------------------
class IMenuController {
public:
    virtual void moveUp()    = 0;
    virtual void moveDown()  = 0;
    virtual void select()    = 0;
    virtual void clearError()= 0;
    virtual bool hasError()  const = 0;
    virtual bool inValueAdjustMode() const = 0;  // still needed for bool/numeric fast-path
    virtual ~IMenuController() {}
};

// -----------------------------------------------------------------------------
// InputManager — abstract base
// -----------------------------------------------------------------------------
class InputManager {
protected:
    IMenuController*  ctrl;
    unsigned long     lastDebounceTime;
    unsigned long     debounceDelay;

public:
    InputManager(IMenuController* c, unsigned long debounce = 50)
        : ctrl(c), lastDebounceTime(0), debounceDelay(debounce) {}

    virtual void update() = 0;
    virtual ~InputManager() {}
};

// -----------------------------------------------------------------------------
// ButtonInputManager
// -----------------------------------------------------------------------------
class ButtonInputManager : public InputManager {
private:
    int upPin, downPin, okPin;
    ButtonTriggerType upTrigger, downTrigger, okTrigger;
    bool upState, downState, okState;
    bool lastUpState, lastDownState, lastOkState;

    bool read(int pin, ButtonTriggerType t) {
        return (t == TRIGGER_LOW) ? (digitalRead(pin) == LOW)
                                  : (digitalRead(pin) == HIGH);
    }

public:
    ButtonInputManager(IMenuController* c, int up, int down, int ok)
        : InputManager(c),
          upPin(up), downPin(down), okPin(ok),
          upTrigger(TRIGGER_LOW), downTrigger(TRIGGER_LOW), okTrigger(TRIGGER_LOW),
          upState(false), downState(false), okState(false),
          lastUpState(false), lastDownState(false), lastOkState(false) {
        pinMode(upPin,   INPUT_PULLUP);
        pinMode(downPin, INPUT_PULLUP);
        pinMode(okPin,   INPUT_PULLUP);
    }

    void setTriggerTypes(ButtonTriggerType up, ButtonTriggerType down, ButtonTriggerType ok) {
        upTrigger = up; downTrigger = down; okTrigger = ok;
    }

    void update() override {
        unsigned long now = millis();
        bool curUp   = read(upPin,   upTrigger);
        bool curDown = read(downPin, downTrigger);
        bool curOk   = read(okPin,   okTrigger);

        // UP
        if (curUp != lastUpState) lastDebounceTime = now;
        if ((now - lastDebounceTime) > debounceDelay && curUp != upState) {
            upState = curUp;
            if (upState) ctrl->moveUp();
        }

        // DOWN
        if (curDown != lastDownState) lastDebounceTime = now;
        if ((now - lastDebounceTime) > debounceDelay && curDown != downState) {
            downState = curDown;
            if (downState) ctrl->moveDown();
        }

        // OK
        if (curOk != lastOkState) lastDebounceTime = now;
        if ((now - lastDebounceTime) > debounceDelay && curOk != okState) {
            okState = curOk;
            if (okState) {
                if (ctrl->hasError()) ctrl->clearError();
                else                  ctrl->select();
            }
        }

        lastUpState   = curUp;
        lastDownState = curDown;
        lastOkState   = curOk;
    }
};

#if MENU_ENABLE_ENCODER
// -----------------------------------------------------------------------------
// EncoderInputManager
// -----------------------------------------------------------------------------
class EncoderInputManager : public InputManager {
private:
#ifdef USE_ESP32_ENCODER
    ESP32Encoder* encoder;
#else
    InterruptEncoder* encoder;
#endif
    int pinA, pinB, buttonPin;
    ButtonTriggerType buttonTrigger;
    bool buttonState, lastButtonState;
    long lastEncoderValue;
    int  sensitivity;
    long accumulator;

public:
    EncoderInputManager(IMenuController* c, int encA, int encB, int btn, int sens = 1)
        : InputManager(c),
          pinA(encA), pinB(encB), buttonPin(btn),
          buttonTrigger(TRIGGER_LOW),
          buttonState(false), lastButtonState(false),
          lastEncoderValue(0), sensitivity(sens), accumulator(0) {
#ifdef USE_ESP32_ENCODER
        encoder = new ESP32Encoder();
        ESP32Encoder::useInternalWeakPullResistors = puType::up;
        encoder->attachHalfQuad(pinA, pinB);
        encoder->setCount(0);
#else
        encoder = new InterruptEncoder();
        encoder->attach(pinA, pinB);
#endif
        pinMode(buttonPin, INPUT_PULLUP);
    }

    ~EncoderInputManager() { delete encoder; }

    void setButtonTriggerType(ButtonTriggerType t) { buttonTrigger = t; }

    void update() override {
#ifdef USE_ESP32_ENCODER
        long cur = encoder->getCount();
#else
        long cur = encoder->read();
#endif
        if (cur != lastEncoderValue) {
            int8_t dir = (cur > lastEncoderValue) ? 1 : -1;
            accumulator += dir;
            if (abs(accumulator) >= sensitivity) {
                if (accumulator > 0) ctrl->moveDown();
                else                 ctrl->moveUp();
                accumulator %= sensitivity;
            }
            lastEncoderValue = cur;
        }

        // Button
        bool curBtn = (buttonTrigger == TRIGGER_LOW) ? (digitalRead(buttonPin) == LOW)
                                                      : (digitalRead(buttonPin) == HIGH);
        if (curBtn != lastButtonState) lastDebounceTime = millis();
        if ((millis() - lastDebounceTime) > debounceDelay && curBtn != buttonState) {
            buttonState = curBtn;
            if (buttonState) {
                if (ctrl->hasError()) ctrl->clearError();
                else                  ctrl->select();
            }
        }
        lastButtonState = curBtn;
    }
};

#endif // MENU_ENABLE_ENCODER

#if MENU_ENABLE_ONE_BUTTON
// -----------------------------------------------------------------------------
// OneButtonInputManager
// Short press = navigate, long press = select/confirm
// -----------------------------------------------------------------------------
class OneButtonInputManager : public InputManager {
public:
    bool          buttonState;
    bool          buttonPressHandled;
    unsigned long buttonPressStartTime;
    unsigned long longPressThreshold;
    bool          shortPressIsUp;

private:
    int               buttonPin;
    ButtonTriggerType buttonTrigger;
    bool              lastButtonState;

// Uncomment to enable debug output:
// #define DEBUG_ONE_BUTTON
#ifdef DEBUG_ONE_BUTTON
#define OB_PRINT(x)   Serial.print(x)
#define OB_PRINTLN(x) Serial.println(x)
#else
#define OB_PRINT(x)
#define OB_PRINTLN(x)
#endif

public:
    OneButtonInputManager(IMenuController* c, int pin, bool upOnShortPress)
        : InputManager(c),
          buttonState(false), buttonPressHandled(false),
          buttonPressStartTime(0), longPressThreshold(3000),
          shortPressIsUp(upOnShortPress),
          buttonPin(pin), buttonTrigger(TRIGGER_LOW),
          lastButtonState(false) {
        pinMode(buttonPin, INPUT_PULLUP);
    }

    void setTriggerType(ButtonTriggerType t)         { buttonTrigger = t; }
    void setLongPressThreshold(unsigned long ms)     { longPressThreshold = ms; }

    void update() override {
        unsigned long now = millis();
        bool cur = (buttonTrigger == TRIGGER_LOW) ? (digitalRead(buttonPin) == LOW)
                                                  : (digitalRead(buttonPin) == HIGH);

        if (cur != lastButtonState) lastDebounceTime = now;
        if ((now - lastDebounceTime) <= debounceDelay) {
            lastButtonState = cur; return;
        }

        // Press start
        if (cur && !buttonState) {
            buttonPressStartTime = now;
            buttonPressHandled   = false;
            OB_PRINTLN("OneBtn: pressed");
        }

        // Long-press threshold
        if (cur && !buttonPressHandled) {
            if ((now - buttonPressStartTime) >= longPressThreshold) {
                OB_PRINTLN("OneBtn: long press -> select");
                if (ctrl->hasError()) ctrl->clearError();
                else                  ctrl->select();
                buttonPressHandled = true;
            }
        }

        // Short-press release
        if (!cur && buttonState) {
            unsigned long dur = now - buttonPressStartTime;
            if (!buttonPressHandled && dur < longPressThreshold) {
                OB_PRINTLN("OneBtn: short press -> navigate");
                if (shortPressIsUp) ctrl->moveUp();
                else                ctrl->moveDown();
            }
            buttonPressHandled = false;
        }

        buttonState     = cur;
        lastButtonState = cur;
    }
};

#endif // MENU_ENABLE_ONE_BUTTON

#endif // INPUT_MANAGER_H
