// ESP32_MenuSystem.cpp
#include "ESP32_MenuSystem.h"

// Button mode constructor
ESP32_MenuSystem::ESP32_MenuSystem(U8G2* u8g2Display, int upPin, int downPin, int okPin)
    : display(u8g2Display), 
      inputMode(INPUT_BUTTONS),
      buttonUpPin(upPin), 
      buttonDownPin(downPin), 
      buttonOkPin(okPin),
      encoder(nullptr),
      standardFont(u8g2_font_5x8_tr),
      titleFont(u8g2_font_6x12_tr),
      valueFont(u8g2_font_10x20_tr),
      screenWidth(128),
      screenHeight(64) {
    
    // Initialize other parameters
    menuCount = 0;
    currentMenuIndex = 0;
    cursorPosition = 0;
    isValueAdjustMode = false;
    currentValueAdjuster = nullptr;

    // Initialize button trigger types to default (TRIGGER_LOW)
    buttonUpTriggerType = TRIGGER_LOW;
    buttonDownTriggerType = TRIGGER_LOW;
    buttonOkTriggerType = TRIGGER_LOW;
    encoderButtonTriggerType = TRIGGER_LOW;

    // Initial layout calculation
    titleHeight = 12;
    separatorY = 12;
    menuStartY = 22;
    lineHeight = 10;
    menuItemsVisible = 4;
    scrollIndicatorWidth = 3;
    menuItemPadding = 2; 

    displayOffsetX = 0;
    displayOffsetY = 0;
    useDisplayOffset = false;
    
    // Initialize button states
    buttonUpState = false;
    buttonDownState = false;
    buttonOkState = false;
    lastButtonUpState = false;
    lastButtonDownState = false;
    lastButtonOkState = false;
    
    lastDebounceTime = 0;
    debounceDelay = 50; // 50ms debounce
    
    previousMillis = 0;
    interval = 1000; // Default 1 second interval
    
    errorCode = 0;
    errorMessage[0] = '\0';
    
    // Set up button pins
    pinMode(buttonUpPin, INPUT_PULLUP);
    pinMode(buttonDownPin, INPUT_PULLUP);
    pinMode(buttonOkPin, INPUT_PULLUP);
}

// Encoder mode constructor
ESP32_MenuSystem::ESP32_MenuSystem(U8G2* u8g2Display, int encoderA, int encoderB, int encoderBtn, bool useEncoder, int sensitivity)
    : display(u8g2Display), 
      inputMode(INPUT_ENCODER),
      encoderPinA(encoderA), 
      encoderPinB(encoderB), 
      encoderButtonPin(encoderBtn),
      encoderSensitivity(sensitivity),
      encoderAccumulator(0),
      standardFont(u8g2_font_5x8_tr),
      titleFont(u8g2_font_6x12_tr),
      valueFont(u8g2_font_10x20_tr),
      screenWidth(128),
      screenHeight(64) {
    
    menuCount = 0;
    currentMenuIndex = 0;
    cursorPosition = 0;
    isValueAdjustMode = false;
    currentValueAdjuster = nullptr;

    // Initialize button trigger types to default (TRIGGER_LOW)
    buttonUpTriggerType = TRIGGER_LOW;
    buttonDownTriggerType = TRIGGER_LOW;
    buttonOkTriggerType = TRIGGER_LOW;
    encoderButtonTriggerType = TRIGGER_LOW;

    // Initial layout calculation based on default values
    titleHeight = 12;
    separatorY = 12;
    menuStartY = 22;
    lineHeight = 10;
    menuItemsVisible = 4;
    scrollIndicatorWidth = 3;
    menuItemPadding = 2; 

    displayOffsetX = 0;
    displayOffsetY = 0;
    useDisplayOffset = false;
    
    // Initialize encoder
    #ifdef USE_ESP32_ENCODER
    encoder = new ESP32Encoder();
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder->attachHalfQuad(encoderPinA, encoderPinB);
    encoder->setCount(0);
    #else
    encoder = new InterruptEncoder();
    encoder->attach(encoderPinA, encoderPinB);
    #endif
    lastEncoderValue = 0;
    
    // Initialize encoder button state
    encoderButtonState = false;
    lastEncoderButtonState = false;
    
    lastDebounceTime = 0;
    debounceDelay = 50; // 50ms debounce
    
    previousMillis = 0;
    interval = 1000; // Default 1 second interval
    
    errorCode = 0;
    errorMessage[0] = '\0';
    
    // Set up button pin
    pinMode(encoderButtonPin, INPUT_PULLUP);
}

// Implementation of configureButtonTriggers
void ESP32_MenuSystem::configureButtonTriggers(ButtonTriggerType upTrigger,
                                               ButtonTriggerType downTrigger,
                                               ButtonTriggerType okTrigger,
                                               ButtonTriggerType encoderTrigger)
{
    buttonUpTriggerType = upTrigger;
    buttonDownTriggerType = downTrigger;
    buttonOkTriggerType = okTrigger;
    encoderButtonTriggerType = encoderTrigger;
}

// Implementation of setButtonTrigger
void ESP32_MenuSystem::setButtonTrigger(ButtonID buttonId, ButtonTriggerType triggerType)
{
    switch (buttonId)
    {
    case BUTTON_ID_UP:
        buttonUpTriggerType = triggerType;
        break;
    case BUTTON_ID_DOWN:
        buttonDownTriggerType = triggerType;
        break;
    case BUTTON_ID_OK:
        buttonOkTriggerType = triggerType;
        break;
    case BUTTON_ID_ENCODER:
        encoderButtonTriggerType = triggerType;
        break;
    }
}

// Destructor
// Update the destructor in ESP32_MenuSystem.cpp
ESP32_MenuSystem::~ESP32_MenuSystem() {
    // Clean up encoder if allocated
    if (encoder != nullptr) {
        delete encoder;
    }
    
    // Clean up any dynamically allocated callbacks
    for (int i = 0; i < menuCount; i++) {
        for (int j = 0; j < menus[i].itemCount; j++) {
            if (menus[i].items[j].callback != nullptr) {
                delete menus[i].items[j].callback;
                menus[i].items[j].callback = nullptr;
            }
        }
    }
}

int ESP32_MenuSystem::addMenu(const char* title) {
    if (menuCount < MAX_MENU_DEPTH) {
        menus[menuCount] = Menu(title, menuCount);
        menuCount++;
        return menuCount - 1;
    }
    return -1;
}

void ESP32_MenuSystem::addMenuItem(int menuIndex, const char* name, int nextMenuId, MenuCallback* callback) {
    if (menuIndex >= 0 && menuIndex < menuCount) {
        menus[menuIndex].addItem(name, nextMenuId, callback);
    }
}

void ESP32_MenuSystem::addValueMenuItem(int menuIndex, const char* name, ValueAdjuster* adjuster) {
    if (menuIndex >= 0 && menuIndex < menuCount) {
        menus[menuIndex].addItem(name, -1, nullptr, adjuster);
    }
}

void ESP32_MenuSystem::setMenuMaxVisibleItems(int menuIndex, int maxItems) {
    if (menuIndex >= 0 && menuIndex < menuCount) {
        menus[menuIndex].maxVisibleItems = maxItems;
    }
}

// Calculate the height of a font
uint8_t ESP32_MenuSystem::getFontHeight(const uint8_t* font) {
    display->setFont(font);
    return display->getMaxCharHeight();
}

// Calculate the approximate width of a character in the font
uint8_t ESP32_MenuSystem::getFontWidth(const uint8_t* font) {
    display->setFont(font);
    // For most monospaced fonts, this will be accurate
    // For variable width fonts, this is an approximation
    return display->getMaxCharWidth();
}

// Update layout based on current font sizes
void ESP32_MenuSystem::updateLayoutForFonts() {
    // Get heights of the fonts
    uint8_t stdFontHeight = getFontHeight(standardFont);
    uint8_t titleFontHeight = getFontHeight(titleFont);
    
    // Adjust title height based on the title font
    titleHeight = titleFontHeight + 2; // Add a little padding
    
    // Separator follows directly after title
    separatorY = titleHeight;
    
    // Menu starts below the separator with some padding
    menuStartY = separatorY + 6;
    
    // Line height is based on standard font height plus the specified padding
    lineHeight = stdFontHeight + menuItemPadding;
    
    // Calculate how many menu items can fit on the screen
    int availableHeight = screenHeight - menuStartY;
    menuItemsVisible = availableHeight / lineHeight;
    
    // Ensure at least one item is visible
    if (menuItemsVisible < 1) menuItemsVisible = 1;
}

// Update screen size to also consider fonts
void ESP32_MenuSystem::setScreenSize(uint16_t width, uint16_t height) {
    screenWidth = width;
    screenHeight = height;
    
    // Set initial layout parameters based on screen size
    titleHeight = (height < 64) ? 10 : ((height >= 128) ? 16 : 12);
    separatorY = titleHeight;
    menuStartY = separatorY + 10;
    lineHeight = (height <= 32) ? 8 : ((height >= 128) ? 12 : 10);
    
    // Calculate visible items
    int availableHeight = screenHeight - menuStartY;
    menuItemsVisible = availableHeight / lineHeight;
    
    // Ensure at least one item is visible
    if (menuItemsVisible < 1) menuItemsVisible = 1;
    
    // Scroll indicator width
    scrollIndicatorWidth = 3;
}

void ESP32_MenuSystem::setDisplayOffset(int16_t x, int16_t y) {
    displayOffsetX = x;
    displayOffsetY = y;
    useDisplayOffset = true;
}

void ESP32_MenuSystem::clearDisplayOffset() {
    displayOffsetX = 0;
    displayOffsetY = 0;
    useDisplayOffset = false;
}

void ESP32_MenuSystem::setLayoutParameters(uint8_t titleH, uint8_t sepY, uint8_t startY, uint8_t lineH) {
    titleHeight = titleH;
    separatorY = sepY;
    menuStartY = startY;
    lineHeight = lineH;
    
    // Calculate appropriate padding based on line height and font height
    uint8_t stdFontHeight = getFontHeight(standardFont);
    menuItemPadding = (lineH > stdFontHeight) ? (lineH - stdFontHeight) : 0;
}

// Update the moveUp method for BoolValueAdjuster
void ESP32_MenuSystem::moveUp() {
    if (isValueAdjustMode && currentValueAdjuster) {
        if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
            // Set temp value to true for boolean adjuster
            BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(currentValueAdjuster);
            boolAdjuster->setTempValue(true);
            return;
        }
        
        // Original value adjust handling
        if (currentValueAdjuster) {
            float value = currentValueAdjuster->getValue();
            float increment = currentValueAdjuster->getIncrement();
            currentValueAdjuster->setValue(value + increment);
        }
    } else {
        // Original menu navigation
        if (cursorPosition > 0) {
            cursorPosition--;
        } else {
            // Wrap around to bottom
            Menu* currentMenu = getCurrentMenu();
            if (currentMenu) {
                cursorPosition = currentMenu->itemCount - 1;
            }
        }
    }
}

// Update the moveDown method
void ESP32_MenuSystem::moveDown() {
    if (isValueAdjustMode && currentValueAdjuster) {
        if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
            // Set temp value to false for boolean adjuster
            BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(currentValueAdjuster);
            boolAdjuster->setTempValue(false);
            return;
        }
        
        // Original value adjust handling
        if (currentValueAdjuster) {
            float value = currentValueAdjuster->getValue();
            float increment = currentValueAdjuster->getIncrement();
            currentValueAdjuster->setValue(value - increment);
        }
    } else {
        // Original menu navigation
        Menu* currentMenu = getCurrentMenu();
        if (currentMenu) {
            if (cursorPosition < currentMenu->itemCount - 1) {
                cursorPosition++;
            } else {
                // Wrap around to top
                cursorPosition = 0;
            }
        }
    }
}

void ESP32_MenuSystem::select() {
    Menu* currentMenu = getCurrentMenu();
    if (currentMenu && cursorPosition < currentMenu->itemCount) {
        MenuItem* selectedItem = &currentMenu->items[cursorPosition];
        
        // Check if this is a value adjustment item
        if (selectedItem->valueAdjuster != nullptr) {
            // Enter value adjustment mode
            enterValueAdjustMode(selectedItem->valueAdjuster);
        } else {
            // Execute callback if it exists
            if (selectedItem->callback) {
                selectedItem->callback->execute();
            }
            
            // Navigate to next menu if specified
            if (selectedItem->nextMenuId >= 0) {
                int nextMenuIndex = findMenuById(selectedItem->nextMenuId);
                if (nextMenuIndex >= 0) {
                    currentMenuIndex = nextMenuIndex;
                    cursorPosition = 0; // Reset cursor position for new menu
                }
            }
        }
    }
}

void ESP32_MenuSystem::goBack() {
    if (currentMenuIndex > 0) {
        currentMenuIndex = 0; // Go back to main menu
        cursorPosition = 0;
    }
}

void ESP32_MenuSystem::setCurrentMenu(int menuId) {
    int menuIndex = findMenuById(menuId);
    if (menuIndex >= 0) {
        currentMenuIndex = menuIndex;
        cursorPosition = 0;
    }
}

void ESP32_MenuSystem::checkButtons() {
    // Only applicable in button mode
    if (inputMode != INPUT_BUTTONS) return;
    
    // Read the current state of buttons based on their individual trigger types
    bool currentUpState = (buttonUpTriggerType == TRIGGER_LOW) ? 
                         (digitalRead(buttonUpPin) == LOW) : 
                         (digitalRead(buttonUpPin) == HIGH);
                         
    bool currentDownState = (buttonDownTriggerType == TRIGGER_LOW) ? 
                           (digitalRead(buttonDownPin) == LOW) : 
                           (digitalRead(buttonDownPin) == HIGH);
                           
    bool currentOkState = (buttonOkTriggerType == TRIGGER_LOW) ? 
                         (digitalRead(buttonOkPin) == LOW) : 
                         (digitalRead(buttonOkPin) == HIGH);
    
    unsigned long currentMillis = millis();
    
    // Check UP button
    if (currentUpState != lastButtonUpState) {
        lastDebounceTime = currentMillis;
    }
    
    if ((currentMillis - lastDebounceTime) > debounceDelay) {
        if (currentUpState != buttonUpState) {
            buttonUpState = currentUpState;
            if (buttonUpState) {
                if (isValueAdjustMode && currentValueAdjuster != nullptr) {
                    // Increase value
                    float value = currentValueAdjuster->getValue();
                    float increment = currentValueAdjuster->getIncrement();
                    currentValueAdjuster->setValue(value + increment);
                } else {
                    moveUp();
                }
            }
        }
    }
    
    // Check DOWN button
    if (currentDownState != lastButtonDownState) {
        lastDebounceTime = currentMillis;
    }
    
    if ((currentMillis - lastDebounceTime) > debounceDelay) {
        if (currentDownState != buttonDownState) {
            buttonDownState = currentDownState;
            if (buttonDownState) {
                if (isValueAdjustMode && currentValueAdjuster != nullptr) {
                    // Decrease value
                    float value = currentValueAdjuster->getValue();
                    float increment = currentValueAdjuster->getIncrement();
                    currentValueAdjuster->setValue(value - increment);
                } else {
                    moveDown();
                }
            }
        }
    }
    
    // Check OK button
    if (currentOkState != lastButtonOkState) {
        lastDebounceTime = currentMillis;
    }
    
    if ((currentMillis - lastDebounceTime) > debounceDelay) {
        if (currentOkState != buttonOkState) {
            buttonOkState = currentOkState;
            if (buttonOkState) {
                if (errorCode > 0) {
                    clearError();
                } else if (isValueAdjustMode && currentValueAdjuster) {
                    // Check for boolean adjuster
                    if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
                        BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(currentValueAdjuster);
                        // Apply the temporary selection to the actual value
                        boolAdjuster->applyTempValue();
                    }
                    exitValueAdjustMode();
                } else {
                    select();
                }
            }
        }
    }
    
    lastButtonUpState = currentUpState;
    lastButtonDownState = currentDownState;
    lastButtonOkState = currentOkState;
}

// Update the handleEncoderMovement method in ESP32_MenuSystem.cpp
void ESP32_MenuSystem::handleEncoderMovement() {
    // Only applicable in encoder mode
    if (inputMode != INPUT_ENCODER || encoder == nullptr) return;
    
    long currentEncoderValue;
    
    #ifdef USE_ESP32_ENCODER
        currentEncoderValue = encoder->getCount();
    #else
        currentEncoderValue = encoder->read();
    #endif
    
    if (currentEncoderValue != lastEncoderValue) {
        // Determine direction (UP or DOWN)
        int8_t encoderDir = (currentEncoderValue > lastEncoderValue) ? 1 : -1;
        
        // Add to accumulator
        encoderAccumulator += encoderDir;
        
        // Only process if accumulator exceeds sensitivity threshold
        if (abs(encoderAccumulator) >= encoderSensitivity) {
            // Determine the direction based on accumulator
            int8_t movementDir = (encoderAccumulator > 0) ? 1 : -1;
            
            if (isValueAdjustMode && currentValueAdjuster != nullptr) {
                // Special handling for boolean adjusters
                if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
                    BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(currentValueAdjuster);
                    
                    // Change the temp selection based on direction
                    // Clockwise (positive) = true, Counter-clockwise (negative) = false
                    boolAdjuster->setTempValue(movementDir > 0);
                } else {
                    // For non-boolean adjusters, use the original value adjustment
                    float currentValue = currentValueAdjuster->getValue();
                    float increment = currentValueAdjuster->getIncrement();
                    
                    // Apply increment based on direction
                    float newValue = currentValue + (increment * movementDir);
                    
                    // Apply limits and set value
                    currentValueAdjuster->setValue(newValue);
                }
            } else {
                // Regular menu navigation
                if (movementDir > 0) {
                    // Move cursor down
                    moveDown();
                } else {
                    // Move cursor up
                    moveUp();
                }
            }
            
            // Reset accumulator (keep the remainder for smoother sensitivity)
            encoderAccumulator = encoderAccumulator % encoderSensitivity;
        }
        
        // Update last value
        lastEncoderValue = currentEncoderValue;
    }
}

void ESP32_MenuSystem::handleButtonPress() {
    // Only applicable in encoder mode
    if (inputMode != INPUT_ENCODER) return;
    
    // Read the state of the encoder button based on its trigger type
    bool currentButtonState = (encoderButtonTriggerType == TRIGGER_LOW) ? 
                             (digitalRead(encoderButtonPin) == LOW) : 
                             (digitalRead(encoderButtonPin) == HIGH);
    
    // Check if the button state changed
    if (currentButtonState != lastEncoderButtonState) {
        lastDebounceTime = millis();
    }
    
    // Debounce
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (currentButtonState != encoderButtonState) {
            encoderButtonState = currentButtonState;
            
            if (encoderButtonState) {
                if (errorCode > 0) {
                    clearError();
                } else if (isValueAdjustMode && currentValueAdjuster) {
                    // Check for boolean adjuster
                    if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
                        BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(currentValueAdjuster);
                        // Apply the temporary selection to the actual value
                        boolAdjuster->applyTempValue();
                    }
                    exitValueAdjustMode();
                } else {
                    select();
                }
            }
        }
    }
    
    lastEncoderButtonState = currentButtonState;
}

void ESP32_MenuSystem::enterValueAdjustMode(ValueAdjuster* adjuster) {
    isValueAdjustMode = true;
    currentValueAdjuster = adjuster;
    
    // For boolean adjusters, initialize temp value to current value
    if (adjuster->getType() == ADJUSTER_TYPE_BOOL) {
        BoolValueAdjuster* boolAdjuster = static_cast<BoolValueAdjuster*>(adjuster);
        boolAdjuster->setTempValue(boolAdjuster->isTrue());
    }
    
    // If using encoder, reset count to prevent sudden jumps
    if (inputMode == INPUT_ENCODER && encoder != nullptr) {
        #ifdef USE_ESP32_ENCODER
            lastEncoderValue = encoder->getCount();
        #else
            lastEncoderValue = encoder->read();
        #endif
    }
}

void ESP32_MenuSystem::displayMenu() {
    if (errorCode > 0) {
        displayError();
        return;
    }
    
    if (isValueAdjustMode && currentValueAdjuster) {
        // Check adjuster type
        if (currentValueAdjuster->getType() == ADJUSTER_TYPE_BOOL) {
            // Use boolean display 
            displayBoolAdjust();
        } else {
            // Use standard value adjust display for other types
            displayValueAdjust();
        }
        return;
    }
    
    Menu* currentMenu = getCurrentMenu();
    if (!currentMenu) return;
    
    display->clearBuffer();
    display->setFont(titleFont);
    
    // Draw title - apply offset if enabled
    int16_t cursorX = 0;
    int16_t cursorY = titleHeight - 2;
    if (useDisplayOffset) {
        cursorX += displayOffsetX;
        cursorY += displayOffsetY;
    }
    display->setCursor(cursorX, cursorY);
    display->print(currentMenu->title);
    
    // Draw separator line - apply offset if enabled
    int16_t lineX = 0;
    int16_t lineY = separatorY;
    if (useDisplayOffset) {
        lineX += displayOffsetX;
        lineY += displayOffsetY;
    }
    display->drawHLine(lineX, lineY, screenWidth);
    
    display->setFont(standardFont);

    // Determine how many items will be visible
    int visibleItems = menuItemsVisible;  // Default (calculated based on screen size)
    
    // If the menu has a custom max visible items setting, use that instead
    if (currentMenu->maxVisibleItems > 0 && currentMenu->maxVisibleItems < visibleItems) {
        visibleItems = currentMenu->maxVisibleItems;
    }
    
    // Calculate which items to display (for scrolling support)
    int displayStart = 0;
    
    // If cursor position is outside visible area, adjust start position
    if (cursorPosition >= visibleItems) {
        displayStart = cursorPosition - visibleItems + 1;
    }
    
    // Make sure we don't try to display past the end
    if (displayStart + visibleItems > currentMenu->itemCount) {
        displayStart = currentMenu->itemCount - visibleItems;
        if (displayStart < 0) displayStart = 0;
    }
    
    // Display the visible items
    for (int i = 0; i < visibleItems && (i + displayStart) < currentMenu->itemCount; i++) {
        int itemIndex = i + displayStart;
        
        // Calculate Y position with minimal spacing
        int16_t yPos = menuStartY + i * (getFontHeight(standardFont) + menuItemPadding);  // Minimal 1px padding
        // ...menuItemPadding
        int16_t leftX = 0;
        int16_t indentedX = 10;
        
        if (useDisplayOffset) {
            yPos += displayOffsetY;
            leftX += displayOffsetX;
            indentedX += displayOffsetX;
        }
        
        if (itemIndex == cursorPosition) {
            display->setCursor(leftX, yPos);
            display->print("> ");
        } else {
            display->setCursor(indentedX, yPos);
        }
        display->print(currentMenu->items[itemIndex].name);
        
        // If this item has a value adjuster, show the current value
        if (currentMenu->items[itemIndex].valueAdjuster != nullptr) {
            ValueAdjuster* adjuster = currentMenu->items[itemIndex].valueAdjuster;
            float value = adjuster->getValue();
            int decimals = adjuster->getDecimalPlaces();
            const char* unit = adjuster->getUnit();
            
            // Format value string based on decimals
            char valueStr[20];
            if (decimals > 0) {
                char format[10];
                sprintf(format, "%%.%df", decimals);
                sprintf(valueStr, format, value);
            } else {
                sprintf(valueStr, "%.0f", value);
            }
            
            // Display value at right side of screen - adjust for screen width
            int16_t xPos = screenWidth - (strlen(valueStr) + strlen(unit) + 1) * 6;
            if (useDisplayOffset) {
                xPos += displayOffsetX;
            }
            display->setCursor(xPos, yPos);
            display->print(valueStr);
            display->print(unit);
        }
    }

    // Draw scroll indicator if needed (if there are more items than visible)
    bool needScrolling = currentMenu->itemCount > visibleItems;
    if (needScrolling) {
        // Calculate scroll bar height and position
        int availableScrollHeight = visibleItems * lineHeight;
        int scrollBarHeight = (visibleItems * availableScrollHeight) / currentMenu->itemCount;
        if (scrollBarHeight < 4) scrollBarHeight = 4; // Minimum size
        
        int16_t scrollBarY = menuStartY;
        if (currentMenu->itemCount > 1) {  // Avoid division by zero
            scrollBarY += (cursorPosition * (availableScrollHeight - scrollBarHeight)) / 
                          (currentMenu->itemCount - 1);
        }
        
        // Apply offset if enabled
        int16_t scrollBarX = screenWidth - scrollIndicatorWidth;
        int16_t startY = menuStartY - (getFontHeight(standardFont) / 2);
        
        if (useDisplayOffset) {
            scrollBarX += displayOffsetX;
            scrollBarY += displayOffsetY;
            startY += displayOffsetY;
        }
        
        // Draw scroll bar at the right edge
        display->drawVLine(scrollBarX, startY, availableScrollHeight);
        display->drawBox(scrollBarX, scrollBarY, scrollIndicatorWidth, scrollBarHeight);
    }

    // Get the selected item, if any
    MenuItem* selectedItem = nullptr;
    if (cursorPosition < currentMenu->itemCount) {
        selectedItem = &currentMenu->items[cursorPosition];
    }

    // Call screen info callback if available
    if (currentMenu->hasScreenInfo && currentMenu->screenInfoCallback) {
        // The callback function can use getDisplayOffsetX() and getDisplayOffsetY() 
        // to apply offsets to its own drawing operations if needed
        currentMenu->screenInfoCallback();
    }

    display->sendBuffer();
}

void ESP32_MenuSystem::addScreenInfo(int menuIndex, ScreenInfoCallback callback) {
    if (menuIndex >= 0 && menuIndex < menuCount) {
        menus[menuIndex].setScreenInfoCallback(callback);
    }
}

void ESP32_MenuSystem::exitValueAdjustMode() {
    isValueAdjustMode = false;
    currentValueAdjuster = nullptr;
}

void ESP32_MenuSystem::displayValueAdjust() {
    if (!currentValueAdjuster) {
        exitValueAdjustMode();
        return;
    }
    
    // Get the value and other properties
    float value = currentValueAdjuster->getValue();
    float min = currentValueAdjuster->getMin();
    float max = currentValueAdjuster->getMax();
    float increment = currentValueAdjuster->getIncrement();
    int decimals = currentValueAdjuster->getDecimalPlaces();
    const char* unit = currentValueAdjuster->getUnit();
    
    display->clearBuffer();
    
    // Title - apply offset if enabled
    display->setFont(titleFont);
    int16_t titleX = 0;
    int16_t titleY = titleHeight - 2;
    if (useDisplayOffset) {
        titleX += displayOffsetX;
        titleY += displayOffsetY;
    }
    display->setCursor(titleX, titleY);
    display->print("Adjust Value");
    
    // Separator line - apply offset if enabled
    int16_t sepX = 0;
    int16_t sepY = separatorY;
    if (useDisplayOffset) {
        sepX += displayOffsetX;
        sepY += displayOffsetY;
    }
    display->drawHLine(sepX, sepY, screenWidth);
    
    // Calculate dimensions for slider
    int sliderY = screenHeight * 0.75;  // 75% down the screen
    int sliderWidth = screenWidth * 0.85;  // 85% of screen width
    int sliderX = (screenWidth - sliderWidth) / 2;  // Center horizontally
    
    // Apply offset to slider if enabled
    if (useDisplayOffset) {
        sliderX += displayOffsetX;
        sliderY += displayOffsetY;
    }
    
    // Show current value in large font
    display->setFont(valueFont);
    
    // Format value string based on decimals
    char valueStr[20];
    if (decimals > 0) {
        char format[10];
        sprintf(format, "%%.%df", decimals);
        sprintf(valueStr, format, value);
    } else {
        sprintf(valueStr, "%.0f", value);
    }
    
    // Center the value display and apply offset if enabled
    int valueY = screenHeight * 0.55;  // 55% down the screen
    int strWidth = strlen(valueStr) * 10;  // Approximate width
    int xPos = (screenWidth - strWidth) / 2;
    if (useDisplayOffset) {
        xPos += displayOffsetX;
        valueY += displayOffsetY;
    }
    display->setCursor(xPos, valueY);
    display->print(valueStr);
    
    // Unit text with offset
    display->setFont(standardFont);
    int16_t unitX = xPos + strWidth + 2;
    int16_t unitY = valueY;
    display->setCursor(unitX, unitY);
    display->print(unit);
    
    // Range indicator (horizontal line)
    display->drawHLine(sliderX, sliderY, sliderWidth);
    
    // Calculate position marker based on value's position in range
    int markerPos = sliderX + (int)(sliderWidth * (value - min) / (max - min));
    
    // Draw marker box with offset already applied to markerPos
    display->drawBox(markerPos - 2, sliderY - 2, 5, 5);
    
    // Min and max values with offset
    display->setFont(standardFont);
    
    char minStr[10], maxStr[10];
    if (decimals > 0) {
        char format[10];
        sprintf(format, "%%.%df", decimals);
        sprintf(minStr, format, min);
        sprintf(maxStr, format, max);
    } else {
        sprintf(minStr, "%.0f", min);
        sprintf(maxStr, "%.0f", max);
    }
    
    // Min value with offset
    int16_t minX = sliderX;
    int16_t minY = sliderY + 10;
    display->setCursor(minX, minY);
    display->print(minStr);
    
    // Max value with offset
    int maxWidth = strlen(maxStr) * 6;  // Approximate width of text
    int16_t maxX = sliderX + sliderWidth - maxWidth;
    int16_t maxY = sliderY + 10;
    display->setCursor(maxX, maxY);
    display->print(maxStr);
    
    display->sendBuffer();
}


// Update the displayBoolAdjust method
void ESP32_MenuSystem::displayBoolAdjust()
{
    // Safe cast - we already checked type in displayMenu
    BoolValueAdjuster *boolAdjuster = static_cast<BoolValueAdjuster *>(currentValueAdjuster);

    display->clearBuffer();

    // Title
    display->setFont(titleFont);

    int16_t titleX = 0;
    int16_t titleY = titleHeight - 2;
    if (useDisplayOffset){
        titleX += displayOffsetX;
        titleY += displayOffsetY;
    }

    // Get the current menu item name
    Menu *currentMenu = getCurrentMenu();
    if (currentMenu && cursorPosition < currentMenu->itemCount)
    {
        display->setCursor(titleX, titleY); // Adjusted based on titleHeight
        display->print(currentMenu->items[cursorPosition].name);
    } else {
        display->setCursor(titleX, titleY); // Adjusted based on titleHeight
        display->print("Boolean Setting");
    }

    display->setFont(standardFont);

    // Draw separator
    int16_t sepX = 0;
    int16_t sepY = separatorY;
    if (useDisplayOffset){
        sepX += displayOffsetX;
        sepY += displayOffsetY;
    }
    display->drawHLine(sepX, sepY, screenWidth);

    // Calculate Y positions based on screen height
    int currentValueY = separatorY + (screenHeight - separatorY) * 0.25; // 25% down usable area
    int optionsStartY = separatorY + (screenHeight - separatorY) * 0.6;  // 60% down usable area
    int optionSpacing = (screenHeight - optionsStartY) / 3;              // Divide remaining space

    // Apply offsets if enabled
    int16_t textX = 0;
    int16_t indentedX = 10;
    if (useDisplayOffset){
        textX += displayOffsetX;
        indentedX += displayOffsetX;
        currentValueY += displayOffsetY;
        optionsStartY += displayOffsetY;
        // Note: optionSpacing doesn't need offset as it's a relative measurement
    }

    // Show current stored value with offset
    display->setCursor(textX, currentValueY);
    display->print("Current value");
    display->setCursor(textX, currentValueY + 10);
    display->print("is set to ");
    display->print(boolAdjuster->getCurrentLabel());

    // Show options with selection indicator
    bool tempValue = boolAdjuster->getTempValue();

    // True option with selection indicator
    if (tempValue)
    {
        display->setCursor(textX, optionsStartY);
        display->print("> ");
    } else {
        display->setCursor(indentedX, optionsStartY);
    }
    display->print(boolAdjuster->getTrueLabel());

    // False option with selection indicator
    if (!tempValue){
        display->setCursor(textX, optionsStartY + optionSpacing);
        display->print("> ");
    } else {
        display->setCursor(indentedX, optionsStartY + optionSpacing);
    }
    display->print(boolAdjuster->getFalseLabel());
    display->sendBuffer();
}

void ESP32_MenuSystem::setError(int code, const char* message) {
    errorCode = code;
    strncpy(errorMessage, message, sizeof(errorMessage) - 1);
    errorMessage[sizeof(errorMessage) - 1] = '\0';
}

void ESP32_MenuSystem::clearError() {
    errorCode = 0;
    errorMessage[0] = '\0';
}

void ESP32_MenuSystem::displayError() {
    display->clearBuffer();
    display->setFont(titleFont);  // Use title font
    
    display->setCursor(0, 20);
    display->print("ERROR #");
    display->print(errorCode);
    
    display->setCursor(0, 35);
    display->print(errorMessage);
    
    display->setCursor(0, 50);
    display->print("Press button to continue");
    
    display->sendBuffer();
}

void ESP32_MenuSystem::update() {
    // Handle input based on mode
    if (inputMode == INPUT_BUTTONS) {
        checkButtons();
    } else if (inputMode == INPUT_ENCODER) {
        handleEncoderMovement();
        handleButtonPress();
    }
    
    // Display the current menu
    displayMenu();
    
    // Add any other periodic updates here
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        // Add any code that needs to run on the interval
    }
}

int ESP32_MenuSystem::findMenuById(int id) {
    for (int i = 0; i < menuCount; i++) {
        if (menus[i].id == id) {
            return i;
        }
    }
    return -1;
}

Menu* ESP32_MenuSystem::getCurrentMenu() {
    if (currentMenuIndex >= 0 && currentMenuIndex < menuCount) {
        return &menus[currentMenuIndex];
    }
    return nullptr;
}

int ESP32_MenuSystem::getCurrentMenuId() const {
    // Get the current menu
    if (currentMenuIndex >= 0 && currentMenuIndex < menuCount) {
        return menus[currentMenuIndex].id;
    }
    return -1;  // Return -1 if there's no valid current menu
}

void ESP32_MenuSystem::begin() {
    // Update layout based on actual font dimensions now that the display is ready
    updateLayoutForFonts();
    
    // If user has set a custom screen size, use those dimensions
    if (display != nullptr) {
        // Get actual display dimensions from U8G2 if possible
        // Not all U8G2 displays provide this info reliably, so we may still need
        // explicit setScreenSize() calls for some displays
        uint16_t displayWidth = display->getDisplayWidth();
        uint16_t displayHeight = display->getDisplayHeight();
        
        // Only update if we got valid dimensions
        if (displayWidth > 0 && displayHeight > 0) {
            screenWidth = displayWidth;
            screenHeight = displayHeight;
            
            // Recalculate layout with new dimensions
            setScreenSize(screenWidth, screenHeight);
        }
    }
}
