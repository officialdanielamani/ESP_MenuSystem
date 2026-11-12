// MenuSystem.h
// Thin orchestrator: navigation state machine + public API.
// Wires DisplayManager, WidgetRenderer, and InputManager together.
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "MenuTypes.h"
#include "Adjusters.h"
#include "DisplayManager.h"
#include "WidgetRenderer.h"
#include "InputManager.h"
#include "DateAdjuster.h"
#include "TimeAdjuster.h"
#include "DisplayGauge.h"
#include "DisplayBar.h"
#include "StringAdjuster.h"

class MenuSystem : public IMenuController {
private:
    // ── Menu tree state ───────────────────────────────────────────────────────
    Menu menus[MAX_MENU_DEPTH];
    int  menuCount;
    int  currentMenuIndex;
    int  cursorPosition;

    // ── Sub-system instances ──────────────────────────────────────────────────
    DisplayManager  dm;
    WidgetRenderer  wr;
    InputManager*   inputMgr;
    InputMode       inputMode;

    // ── Editor state ──────────────────────────────────────────────────────────
    bool                inValueEdit;
    ValueAdjuster*      currentAdjuster;
    bool                inMultiSelect;
    MultiSelectAdjuster* currentMultiSelect;

    // ── Date editor state ─────────────────────────────────────────────────────
    bool          inDateEdit;
    DateAdjuster* currentDateAdj;

    // ── Time editor state ─────────────────────────────────────────────────────
    bool          inTimeEdit;
    TimeAdjuster* currentTimeAdj;

    // ── Gauge view state ──────────────────────────────────────────────────────
    bool              inGaugeView;
    DisplayGaugeBase* currentGauge;

    // ── Bar view state ─────────────────────────────────────────────────────
    bool              inBarView;
    DisplayBarBase*   currentBar;

    // ── String editor state ───────────────────────────────────────────────────
    bool              inStringEdit;
    StringAdjuster*   currentStringAdj;

    // ── Error state ───────────────────────────────────────────────────────────
    int  errorCode;
    char errorMessage[64];

    // ── Timing ────────────────────────────────────────────────────────────────
    unsigned long previousMillis;
    unsigned long interval;

    // ── Name scroll state (for layoutMaxChars) ─────────────────────────────
    int           scrollOffset;         // current character offset into the name
    unsigned long scrollLastUpdate;     // millis() of last scroll step
    int           scrollCursorItem;     // which item index is being scrolled (-1 = none)
    unsigned long scrollPauseUntil;     // millis() timestamp: don't scroll until this time

    // ── IMenuController implementation ───────────────────────────────────────
    bool hasError()           const override { return errorCode > 0; }
    bool inValueAdjustMode()  const override { return inValueEdit; }

public:
    // ── Constructors ──────────────────────────────────────────────────────────

    // Three-button mode
    MenuSystem(U8G2* display, int upPin, int downPin, int okPin)
        : menuCount(0), currentMenuIndex(0), cursorPosition(0),
          dm(display), wr(dm),
          inputMode(INPUT_BUTTONS),
          inValueEdit(false), currentAdjuster(nullptr),
          inMultiSelect(false), currentMultiSelect(nullptr),
          inDateEdit(false), currentDateAdj(nullptr),
          inTimeEdit(false), currentTimeAdj(nullptr),
          inGaugeView(false), currentGauge(nullptr),
          inBarView(false), currentBar(nullptr),
          inStringEdit(false), currentStringAdj(nullptr),
          errorCode(0), previousMillis(0), interval(1000),
          scrollOffset(0), scrollLastUpdate(0), scrollCursorItem(-1), scrollPauseUntil(0) {
        errorMessage[0] = '\0';
        inputMgr = new ButtonInputManager(this, upPin, downPin, okPin);
    }

    // Rotary encoder mode
    MenuSystem(U8G2* display, int encA, int encB, int encBtn,
               bool /*unused*/, int sensitivity = 1)
        : menuCount(0), currentMenuIndex(0), cursorPosition(0),
          dm(display), wr(dm),
          inputMode(INPUT_ENCODER),
          inValueEdit(false), currentAdjuster(nullptr),
          inMultiSelect(false), currentMultiSelect(nullptr),
          inDateEdit(false), currentDateAdj(nullptr),
          inTimeEdit(false), currentTimeAdj(nullptr),
          inGaugeView(false), currentGauge(nullptr),
          inBarView(false), currentBar(nullptr),
          inStringEdit(false), currentStringAdj(nullptr),
          errorCode(0), previousMillis(0), interval(1000),
          scrollOffset(0), scrollLastUpdate(0), scrollCursorItem(-1), scrollPauseUntil(0) {
        errorMessage[0] = '\0';
        inputMgr = new EncoderInputManager(this, encA, encB, encBtn, sensitivity);
    }

    // Single-button mode
    MenuSystem(U8G2* display, int buttonPin, bool shortPressIsUp = true)
        : menuCount(0), currentMenuIndex(0), cursorPosition(0),
          dm(display), wr(dm),
          inputMode(INPUT_ONE_BUTTON),
          inValueEdit(false), currentAdjuster(nullptr),
          inMultiSelect(false), currentMultiSelect(nullptr),
          inDateEdit(false), currentDateAdj(nullptr),
          inTimeEdit(false), currentTimeAdj(nullptr),
          inGaugeView(false), currentGauge(nullptr),
          inBarView(false), currentBar(nullptr),
          inStringEdit(false), currentStringAdj(nullptr),
          errorCode(0), previousMillis(0), interval(1000),
          scrollOffset(0), scrollLastUpdate(0), scrollCursorItem(-1), scrollPauseUntil(0) {
        errorMessage[0] = '\0';
        inputMgr = new OneButtonInputManager(this, buttonPin, shortPressIsUp);
    }

    ~MenuSystem() {
        delete inputMgr;
        for (int i = 0; i < menuCount; i++)
            for (int j = 0; j < menus[i].itemCount; j++) {
                if (menus[i].items[j].callback) {
                    delete menus[i].items[j].callback;
                    menus[i].items[j].callback = nullptr;
                }
                if (menus[i].items[j].textInfo) {
                    delete menus[i].items[j].textInfo;
                    menus[i].items[j].textInfo = nullptr;
                }
            }
    }

    // ── Initialise ────────────────────────────────────────────────────────────
    void begin() { dm.begin(); }

    // ── Main loop call ────────────────────────────────────────────────────────
    void update() { inputMgr->update(); render(); }

    // ── Menu building ─────────────────────────────────────────────────────────
    int addMenu(const char* title) {
        if (menuCount >= MAX_MENU_DEPTH) return -1;
        menus[menuCount] = Menu(title, menuCount);
        return menuCount++;
    }

    void addMenuItem(int menuIdx, const char* name,
                     int nextMenuId = -1, MenuCallback* cb = nullptr) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, nextMenuId, cb);
    }

    void addMenuItemWithFunction(int menuIdx, const char* name,
                                 SimpleMenuFunction fn, int nextMenuId = -1) {
        if (valid(menuIdx))
            menus[menuIdx].addItem(name, nextMenuId, new FunctionCallback(fn));
    }

    // ── Live rename ──────────────────────────────────────────────────────────
    // Item/menu names are copied into fixed buffers at add-time (see MenuItem
    // and Menu in MenuTypes.h), so changing the caller's original string later
    // does NOT update what's on screen. These two let a caller overwrite an
    // already-added item's label or a menu's title in place, with no effect
    // on the menu tree's structure, callbacks, or the input/encoder state --
    // unlike tearing down and rebuilding the whole MenuSystem, this is safe to
    // call at any time, including from a web request handler.
    // The "public:" right here is deliberate and load-bearing: it makes these
    // two methods public no matter what access section they land in if pasted
    // into a differently-ordered copy of this file, rather than silently
    // inheriting whatever came before (which is what caused this to compile
    // as private the first time).
public:
    bool renameItem(int menuIdx, int itemIdx, const char* newName) {
        if (!valid(menuIdx)) return false;
        if (itemIdx < 0 || itemIdx >= menus[menuIdx].itemCount) return false;
        if (!newName) return false;
        strncpy(menus[menuIdx].items[itemIdx].name, newName, MAX_MENU_NAME_LENGTH - 1);
        menus[menuIdx].items[itemIdx].name[MAX_MENU_NAME_LENGTH - 1] = '\0';
        return true;
    }

    bool renameMenuTitle(int menuIdx, const char* newTitle) {
        if (!valid(menuIdx)) return false;
        if (!newTitle) return false;
        strncpy(menus[menuIdx].title, newTitle, MAX_MENU_NAME_LENGTH - 1);
        menus[menuIdx].title[MAX_MENU_NAME_LENGTH - 1] = '\0';
        return true;
    }

    void addValueMenuItem(int menuIdx, const char* name, ValueAdjuster* adj) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, adj);
    }

    void addDateMenuItem(int menuIdx, const char* name, DateAdjuster* adj) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, adj);
    }

    void addTimeMenuItem(int menuIdx, const char* name, TimeAdjuster* adj) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, adj);
    }

    void addDisplayGaugeInt(int menuIdx, const char* name, DisplayGaugeInt* g) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, g);
    }

    void addDisplayGaugeFloat(int menuIdx, const char* name, DisplayGaugeFloat* g) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, g);
    }

    void addDisplayBarInt(int menuIdx, const char* name, DisplayBarInt* b) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, b);
    }

    void addDisplayBarFloat(int menuIdx, const char* name, DisplayBarFloat* b) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, b);
    }

    void addMultiSelectMenuItem(int menuIdx, const char* name, MultiSelectAdjuster* adj) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, adj);
    }

    void addStringMenuItem(int menuIdx, const char* name, StringAdjuster* adj) {
        if (valid(menuIdx)) menus[menuIdx].addItem(name, -1, nullptr, adj);
    }

    // ── Text rows (non-selectable, display-only) ──────────────────────────────
    //
    // Inserts a non-selectable text row into the menu list. The cursor
    // skips over it. The text source is a const char** so it can track
    // a changing String.c_str() or a static literal.
    //
    //   textPtr    – pointer to a const char* variable holding the text
    //   paragraph  – true: word-wrap across lines; false: single line, scroll
    //   fontType   – TEXT_FONT_DEFAULT / SMALL / LARGE / CUSTOM
    //   maxChars   – chars per visual line (0 = auto from screen width)
    //   ofsX, ofsY – pixel offset override (-1 = auto position)
    //   customFont – U8g2 font pointer (only when fontType == TEXT_FONT_CUSTOM)
    //
    // Examples:
    //   static const char* info = "Power and Temp OK";
    //   menu.addMenuText(settingsMenu, &info);
    //   menu.addMenuText(settingsMenu, &info, true, TEXT_FONT_LARGE, 16);
    //   menu.addMenuText(settingsMenu, &info, false, TEXT_FONT_CUSTOM, 0, -1, -1, u8g2_font_6x12_tr);
    //
    void addMenuText(int menuIdx, const char** textPtr,
                     bool paragraph = false,
                     TextFontType fontType = TEXT_FONT_DEFAULT,
                     uint8_t maxChars = 0,
                     int8_t ofsX = -1, int8_t ofsY = -1,
                     const uint8_t* customFont = nullptr)
    {
        if (!valid(menuIdx)) return;
        if (!textPtr) return;

        // Allocate a MenuTextInfo (lives for the lifetime of the menu system)
        MenuTextInfo* ti = new MenuTextInfo();
        ti->textPtr    = textPtr;
        ti->paragraph  = paragraph;
        ti->fontType   = fontType;
        ti->customFont = customFont;
        ti->maxChars   = (maxChars > MAX_TEXT_LENGTH) ? MAX_TEXT_LENGTH : maxChars;
        ti->ofsX       = ofsX;
        ti->ofsY       = ofsY;
        ti->wrappedLines = 1;

        // The "name" field is unused for text items; store a marker
        menus[menuIdx].addItem("", -1, nullptr, nullptr, ti);
    }

    void setMenuMaxVisibleItems(int menuIdx, int max) {
        if (valid(menuIdx)) menus[menuIdx].setMaxVisibleItems(max);
    }

    void addScreenInfo(int menuIdx, ScreenInfoCallback cb) {
        if (valid(menuIdx)) menus[menuIdx].setScreenInfoCallback(cb);
    }

    // ── Layout per menu ─────────────────────────────────────────────────────
    //   Controls item name display length, visible item count, and anchoring.
    //   If not called, the menu uses the display's default settings.
    //
    //   menu.setMenuLayout(menuId, 8);                    // 8 char name limit
    //   menu.setMenuLayout(menuId, 8, 1);                 // 8 chars, 1 item visible
    //   menu.setMenuLayout(menuId, 8, 1, BOTTOM);         // 8 chars, 1 item, bottom
    //   menu.setMenuLayout(menuId, 8, 1, TOP, false);     // 8 chars, 1 item, no counter
    //   menu.setMenuLayout(menuId, 0, 3);                 // no char limit, 3 items
    //
    //   When maxChars > 0 and a name is longer, non-selected items are
    //   truncated.  The selected item auto-scrolls to reveal the full name.
    void setMenuLayout(int menuIdx, int maxChars = 0, int visibleItems = 0,
                       MenuAnchor anchor = TOP, bool showCounter = true) {
        if (valid(menuIdx)) menus[menuIdx].setLayout(maxChars, visibleItems, anchor, showCounter);
    }

    // ── Style / appearance API ────────────────────────────────────────────────

    // Selection cursor style
    void setSelectionStyle(SelectionStyle s)  { dm.setSelectionStyle(s); }
    SelectionStyle getSelectionStyle()  const { return dm.selectionStyle; }
    void setBlinkInterval(unsigned long ms)   { dm.setBlinkInterval(ms); }

    // Show or hide the horizontal separator line under the title
    void setShowSeparator(bool show)          { dm.setShowSeparator(show); }
    bool getShowSeparator()             const { return dm.showSeparator; }

    // Show or hide the right-edge scroll bar
    void setShowScrollBar(bool show)          { dm.setShowScrollBar(show); }
    bool getShowScrollBar()             const { return dm.showScrollBar; }

    // ── Font API ──────────────────────────────────────────────────────────────
    //
    // Tier 1 — Preset: picks a curated trio, auto-recalculates layout.
    //   menu.setFontPreset(FONT_PRESET_SMALL);   // tiny, max items
    //   menu.setFontPreset(FONT_PRESET_NORMAL);  // balanced default
    //   menu.setFontPreset(FONT_PRESET_LARGE);   // readable, fewer items
    //
    void setFontPreset(FontPreset preset)         { dm.setFontPreset(preset); }

    // Tier 2 — Full control: supply any U8g2 font for each role.
    //   nullptr keeps the current font for that role.
    //   All three at once:
    //     menu.setFonts(u8g2_font_4x6_tr,
    //                   u8g2_font_helvB08_tr,
    //                   u8g2_font_helvB08_tr);
    //   Single role:
    //     menu.setStandardFont(u8g2_font_helvB08_tr);
    //     menu.setTitleFont(u8g2_font_helvB12_tr);
    //     menu.setValueFont(u8g2_font_10x20_tr);
    //
    // setFonts / setStandardFont / setTitleFont auto-recalculate layout.
    // setValueFont does not affect layout (value editor only).
    //
    void setFonts(const uint8_t* stdFont,
                  const uint8_t* titleFont,
                  const uint8_t* valueFont)       { dm.setFonts(stdFont, titleFont, valueFont); }
    void setStandardFont(const uint8_t* f)        { dm.setStandardFont(f); }
    void setTitleFont   (const uint8_t* f)        { dm.setTitleFont(f); }
    void setValueFont   (const uint8_t* f)        { dm.setValueFont(f); }

    // ── DisplayManager passthrough ────────────────────────────────────────────
    void setScreenSize(uint16_t w, uint16_t h)       { dm.setScreenSize(w, h); }
    void setMenuItemPadding(uint8_t p)               { dm.setMenuItemPadding(p); }
    void setAutoItemPadding()                        { dm.setAutoItemPadding(); }
    void setDisplayOffset(int16_t x, int16_t y)      { dm.setOffset(x, y); }
    void clearDisplayOffset()                        { dm.clearOffset(); }
    void setLayoutParameters(uint8_t tH, uint8_t sY,
                             uint8_t startY, uint8_t lH) { dm.setLayoutParameters(tH, sY, startY, lH); }
    uint16_t getScreenWidth()  const                 { return dm.screenWidth; }
    uint16_t getScreenHeight() const                 { return dm.screenHeight; }
    bool     isUsingDisplayOffset() const            { return dm.useOffset; }
    int16_t  getDisplayOffsetX()    const            { return dm.offsetX; }
    int16_t  getDisplayOffsetY()    const            { return dm.offsetY; }

    // ── InputManager passthrough ──────────────────────────────────────────────
    void configureButtonTriggers(ButtonTriggerType up    = TRIGGER_LOW,
                                 ButtonTriggerType down  = TRIGGER_LOW,
                                 ButtonTriggerType ok    = TRIGGER_LOW,
                                 ButtonTriggerType enc   = TRIGGER_LOW) {
        if (inputMode == INPUT_BUTTONS)
            static_cast<ButtonInputManager*>(inputMgr)->setTriggerTypes(up, down, ok);
        else if (inputMode == INPUT_ENCODER)
            static_cast<EncoderInputManager*>(inputMgr)->setButtonTriggerType(enc);
        else if (inputMode == INPUT_ONE_BUTTON)
            static_cast<OneButtonInputManager*>(inputMgr)->setTriggerType(ok);
    }

    void setLongPressThreshold(unsigned long ms) {
        if (inputMode == INPUT_ONE_BUTTON)
            static_cast<OneButtonInputManager*>(inputMgr)->setLongPressThreshold(ms);
    }

    // ── Navigation (public + IMenuController) ────────────────────────────────
    void moveUp() override {
        // While gauge is showing: close it, then let the cursor move normally
        if (inGaugeView) {
            inGaugeView  = false;
            currentGauge = nullptr;
            // fall through — cursor moves up below
        }
        if (inBarView) {
            inBarView  = false;
            currentBar = nullptr;
        }
        if (inTimeEdit && currentTimeAdj) {
            if (currentTimeAdj->isOnField()) currentTimeAdj->scrollUp();
            else currentTimeAdj->actionUp();
            return;
        }
        if (inDateEdit && currentDateAdj) {
            if (currentDateAdj->isOnField()) currentDateAdj->scrollUp();
            else currentDateAdj->actionUp();   // wraps within Redo/Set/Back only
            return;
        }
        if (inStringEdit && currentStringAdj) {
            if (currentStringAdj->isOnConfirm()) currentStringAdj->confirmUp();
            else if (currentStringAdj->isOnChar()) currentStringAdj->scrollUp();
            else currentStringAdj->actionUp();
            return;
        }
        if (inMultiSelect && currentMultiSelect) {
            currentMultiSelect->moveCursorUp(); return;
        }
        if (inValueEdit && currentAdjuster) {
            if (currentAdjuster->getType() == ValueAdjuster::TYPE_BOOL)
                static_cast<BoolValueAdjuster*>(currentAdjuster)->setTempValue(true);
            else
                currentAdjuster->setValue(currentAdjuster->getValue() + currentAdjuster->getIncrement());
            return;
        }
        Menu* m = currentMenu();
        if (!m) return;
        cursorPosition = (cursorPosition > 0) ? cursorPosition - 1 : m->itemCount - 1;
        skipTextItems(-1);
        resetScroll();
        checkAutoOpenGauge();
    }

    void moveDown() override {
        // While gauge is showing: close it, then let the cursor move normally
        if (inGaugeView) {
            inGaugeView  = false;
            currentGauge = nullptr;
            // fall through — cursor moves down below
        }
        if (inBarView) {
            inBarView  = false;
            currentBar = nullptr;
        }
        if (inTimeEdit && currentTimeAdj) {
            if (currentTimeAdj->isOnField()) currentTimeAdj->scrollDown();
            else currentTimeAdj->actionDown();
            return;
        }
        if (inDateEdit && currentDateAdj) {
            if (currentDateAdj->isOnField()) currentDateAdj->scrollDown();
            else currentDateAdj->actionDown(); // wraps within Redo/Set/Back only
            return;
        }
        if (inStringEdit && currentStringAdj) {
            if (currentStringAdj->isOnConfirm()) currentStringAdj->confirmDown();
            else if (currentStringAdj->isOnChar()) currentStringAdj->scrollDown();
            else currentStringAdj->actionDown();
            return;
        }
        if (inMultiSelect && currentMultiSelect) {
            currentMultiSelect->moveCursorDown(); return;
        }
        if (inValueEdit && currentAdjuster) {
            if (currentAdjuster->getType() == ValueAdjuster::TYPE_BOOL)
                static_cast<BoolValueAdjuster*>(currentAdjuster)->setTempValue(false);
            else
                currentAdjuster->setValue(currentAdjuster->getValue() - currentAdjuster->getIncrement());
            return;
        }
        Menu* m = currentMenu();
        if (!m) return;
        cursorPosition = (cursorPosition < m->itemCount - 1) ? cursorPosition + 1 : 0;
        skipTextItems(1);
        resetScroll();
        checkAutoOpenGauge();
    }

    void select() override {
        if (inGaugeView) { inGaugeView = false; currentGauge = nullptr; return; }
        if (inBarView)   { inBarView   = false; currentBar   = nullptr; return; }
        if (inTimeEdit && currentTimeAdj) {
            if (currentTimeAdj->isOnRedo()) {
                currentTimeAdj->redo();
            } else if (currentTimeAdj->isOnEdit()) {
                currentTimeAdj->edit();
            } else if (currentTimeAdj->isOnSave()) {
                currentTimeAdj->confirm();
                inTimeEdit     = false;
                currentTimeAdj = nullptr;
            } else if (currentTimeAdj->isOnBack()) {
                inTimeEdit     = false;
                currentTimeAdj = nullptr;
            } else {
                currentTimeAdj->nextCol();
            }
            return;
        }
        if (inDateEdit && currentDateAdj) {
            if (currentDateAdj->isOnRedo()) {
                currentDateAdj->redo();
            } else if (currentDateAdj->isOnEdit()) {
                currentDateAdj->edit();
            } else if (currentDateAdj->isOnSave()) {
                currentDateAdj->confirm();
                inDateEdit     = false;
                currentDateAdj = nullptr;
            } else if (currentDateAdj->isOnBack()) {
                inDateEdit     = false;
                currentDateAdj = nullptr;
            } else {
                currentDateAdj->nextCol();
            }
            return;
        }
        if (inStringEdit && currentStringAdj) {
            if (currentStringAdj->isOnConfirm()) {
                // Clear All confirm dialog: Yes clears, No cancels
                if (currentStringAdj->confirmIsYes()) {
                    currentStringAdj->clearAll();
                } else {
                    currentStringAdj->cancelConfirmClear();
                }
            } else if (currentStringAdj->isOnChar()) {
                currentStringAdj->selectChar();
            } else if (currentStringAdj->isOnRedo()) {
                currentStringAdj->redo();
            } else if (currentStringAdj->isOnEdit()) {
                currentStringAdj->edit();
            } else if (currentStringAdj->isOnClearAll()) {
                currentStringAdj->beginConfirmClear();
            } else if (currentStringAdj->isOnSave()) {
                if (currentStringAdj->isLengthValid()) {
                    currentStringAdj->confirm();
                    inStringEdit     = false;
                    currentStringAdj = nullptr;
                }
            } else if (currentStringAdj->isOnBack()) {
                inStringEdit     = false;
                currentStringAdj = nullptr;
            }
            return;
        }
        if (inMultiSelect && currentMultiSelect) {
            if (currentMultiSelect->isOnOk()) {
                // Only exit if min selection requirement met (or empty list)
                if (currentMultiSelect->canConfirm()) {
                    inMultiSelect      = false;
                    currentMultiSelect = nullptr;
                }
                // else: stay on OK row, user needs to select more
            } else {
                currentMultiSelect->toggleCurrent();
            }
            return;
        }
        if (inValueEdit && currentAdjuster) {
            if (currentAdjuster->getType() == ValueAdjuster::TYPE_BOOL)
                static_cast<BoolValueAdjuster*>(currentAdjuster)->applyTempValue();
            inValueEdit     = false;
            currentAdjuster = nullptr;
            return;
        }
        Menu* m = currentMenu();
        if (!m || cursorPosition >= m->itemCount) return;
        MenuItem* item = &m->items[cursorPosition];

        // Text items are non-selectable — ignore OK press
        if (item->isTextItem()) return;

        if (item->valueAdjuster) {
            if (item->valueAdjuster->getType() == ValueAdjuster::TYPE_TIME) {
                currentTimeAdj = static_cast<TimeAdjuster*>(item->valueAdjuster);
                currentTimeAdj->beginEdit();
                inTimeEdit = true;
                return;
            }
            if (item->valueAdjuster->getType() == ValueAdjuster::TYPE_DATE) {
                currentDateAdj = static_cast<DateAdjuster*>(item->valueAdjuster);
                currentDateAdj->beginEdit();
                inDateEdit = true;
                return;
            }
            if (item->valueAdjuster->getType() == ValueAdjuster::TYPE_STRING) {
                currentStringAdj = static_cast<StringAdjuster*>(item->valueAdjuster);
                currentStringAdj->beginEdit();
                inStringEdit = true;
                return;
            }
            if (item->valueAdjuster->getType() == ValueAdjuster::TYPE_MULTI) {
                currentMultiSelect = static_cast<MultiSelectAdjuster*>(item->valueAdjuster);
                inMultiSelect      = true;
            } else {
                currentAdjuster = item->valueAdjuster;
                inValueEdit     = true;
                if (currentAdjuster->getType() == ValueAdjuster::TYPE_BOOL)
                    static_cast<BoolValueAdjuster*>(currentAdjuster)
                        ->setTempValue(static_cast<BoolValueAdjuster*>(currentAdjuster)->isTrue());
            }
            return;
        }

        if (item->callback) item->callback->execute();

        if (item->nextMenuId >= 0) {
            int idx = findMenuById(item->nextMenuId);
            if (idx >= 0) { currentMenuIndex = idx; cursorPosition = 0; resetScroll(); skipTextItems(1); }
        }
    }

    void clearError() override { errorCode = 0; errorMessage[0] = '\0'; }

    void goBack() {
        if (currentMenuIndex > 0) { currentMenuIndex = 0; cursorPosition = 0; resetScroll(); skipTextItems(1); }
    }

    void setCurrentMenu(int menuId) {
        int idx = findMenuById(menuId);
        if (idx >= 0) { currentMenuIndex = idx; cursorPosition = 0; resetScroll(); skipTextItems(1); }
    }

    int  getCursorPosition() const { return cursorPosition; }
    int  getCurrentMenuId()  const {
        return (currentMenuIndex >= 0 && currentMenuIndex < menuCount)
               ? menus[currentMenuIndex].id : -1;
    }

    // ── Error handling ────────────────────────────────────────────────────────
    void setError(int code, const char* msg) {
        errorCode = code;
        strncpy(errorMessage, msg, sizeof(errorMessage) - 1);
        errorMessage[sizeof(errorMessage) - 1] = '\0';
    }

    // ── Internals ─────────────────────────────────────────────────────────────
    int   findMenuById(int id) const {
        for (int i = 0; i < menuCount; i++) if (menus[i].id == id) return i;
        return -1;
    }
    Menu* currentMenu() {
        return (currentMenuIndex >= 0 && currentMenuIndex < menuCount)
               ? &menus[currentMenuIndex] : nullptr;
    }

    void resetScroll() {
        scrollOffset     = 0;
        scrollCursorItem = -1;
        scrollLastUpdate = 0;
        scrollPauseUntil = 0;
    }

    // Skip cursor over non-selectable text items.
    // dir: +1 = forward, -1 = backward. Wraps around. Gives up after itemCount tries.
    void skipTextItems(int dir) {
        Menu* m = currentMenu();
        if (!m || m->itemCount == 0) return;
        int tries = m->itemCount;
        while (tries-- > 0 && m->items[cursorPosition].isTextItem()) {
            if (dir > 0)
                cursorPosition = (cursorPosition < m->itemCount - 1) ? cursorPosition + 1 : 0;
            else
                cursorPosition = (cursorPosition > 0) ? cursorPosition - 1 : m->itemCount - 1;
        }
    }

    // Resolve a MenuTextInfo's font type to an actual U8g2 font pointer.
    const uint8_t* resolveTextFont(const MenuTextInfo* ti) const {
        if (!ti) return dm.standardFont;
        switch (ti->fontType) {
            case TEXT_FONT_SMALL:  return u8g2_font_4x6_tr;
            case TEXT_FONT_LARGE:  return u8g2_font_6x12_tr;
            case TEXT_FONT_CUSTOM: return ti->customFont ? ti->customFont : dm.standardFont;
            default:               return dm.standardFont;
        }
    }

private:
    bool valid(int idx) const { return idx >= 0 && idx < menuCount; }

    // Called after every cursor move. If the newly selected item is a gauge
    // or a default-type bar, open the full-screen view immediately (no OK press needed).
    void checkAutoOpenGauge() {
        Menu* m = currentMenu();
        if (!m || cursorPosition >= m->itemCount) return;
        ValueAdjuster* adj = m->items[cursorPosition].valueAdjuster;
        if (adj && adj->getType() == ValueAdjuster::TYPE_GAUGE) {
            currentGauge = static_cast<DisplayGaugeBase*>(adj);
            inGaugeView  = true;
        }
        if (adj && adj->getType() == ValueAdjuster::TYPE_BAR) {
            currentBar  = static_cast<DisplayBarBase*>(adj);
            inBarView   = true;
        }
    }

    // ── Render dispatcher ─────────────────────────────────────────────────────
    void render() {
        dm.updateBlink();  // update blink timer for SELECTION_BLINK
        if (errorCode > 0) {
            wr.drawError(errorCode, errorMessage); return;
        }
        if (inMultiSelect && currentMultiSelect) {
            const char* title = titleForCurrentItem();
            wr.drawMultiSelect(currentMultiSelect, title); return;
        }
        if (inGaugeView && currentGauge) {
            Menu* m = currentMenu();
            const char* menuTitle = m ? m->title : "";
            // Pass horizontal counter info so gauge screen shows "2/5" in title bar
            int cCur = 0, cTot = 0;
            if (m && m->layoutVisibleItems > 0 && m->layoutShowCounter) {
                cCur = cursorPosition + 1;
                cTot = m->itemCount;
            }
            wr.drawGaugeScreen(currentGauge, menuTitle, titleForCurrentItem(), cCur, cTot);
            return;
        }
        if (inBarView && currentBar) {
            Menu* m = currentMenu();
            const char* menuTitle = m ? m->title : "";
            int cCur = 0, cTot = 0;
            if (m && m->layoutVisibleItems > 0 && m->layoutShowCounter) {
                cCur = cursorPosition + 1;
                cTot = m->itemCount;
            }
            wr.drawBarScreen(currentBar, menuTitle, titleForCurrentItem(), cCur, cTot);
            return;
        }
        if (inDateEdit && currentDateAdj) {
            wr.drawDateEditor(currentDateAdj, titleForCurrentItem());
            return;
        }
        if (inTimeEdit && currentTimeAdj) {
            wr.drawTimeEditor(currentTimeAdj, titleForCurrentItem());
            return;
        }
        if (inStringEdit && currentStringAdj) {
            wr.drawStringEditor(currentStringAdj, titleForCurrentItem());
            return;
        }
        if (inValueEdit && currentAdjuster) {
            if (currentAdjuster->getType() == ValueAdjuster::TYPE_BOOL)
                wr.drawBoolEditor(static_cast<BoolValueAdjuster*>(currentAdjuster),
                                  titleForCurrentItem());
            else
                wr.drawNumericEditor(currentAdjuster, titleForCurrentItem());
            return;
        }
        renderMenuList();
    }

    const char* titleForCurrentItem() {
        Menu* m = currentMenu();
        if (m && cursorPosition < m->itemCount)
            return m->items[cursorPosition].name;
        return "";
    }

    void renderMenuList() {
        Menu* m = currentMenu();
        if (!m) return;

        U8G2* d = dm.display;
        d->clearBuffer();

        // Title + separator
        dm.drawTitleBar(m->title);
        dm.drawSeparator();

        // One-button long-press progress bar
        if (inputMode == INPUT_ONE_BUTTON) {
            OneButtonInputManager* ob = static_cast<OneButtonInputManager*>(inputMgr);
            if (ob->buttonState && !ob->buttonPressHandled) {
                unsigned long dur = millis() - ob->buttonPressStartTime;
                int16_t lineY = dm.oy() + dm.separatorY;
                int16_t lineX = dm.ox();
                float   prog  = (float)dur / ob->longPressThreshold;
                if (prog > 1.0f) prog = 1.0f;
                d->drawBox(lineX, lineY + 2, (int)(dm.screenWidth * prog), 2);
            }
        }

        d->setFont(dm.standardFont);

        // ── How many visual rows fit on screen ────────────────────────────────
        int visItems = dm.menuItemsVisible;
        if (m->maxVisibleItems > 0 && m->maxVisibleItems < visItems)
            visItems = m->maxVisibleItems;
        if (m->layoutVisibleItems > 0)
            visItems = m->layoutVisibleItems;

        uint8_t fontH = dm.fontHeight(dm.standardFont);
        uint8_t fw    = dm.fontWidth(dm.standardFont);

        // ── Build visual row map ──────────────────────────────────────────────
        // Each visual row records: which logical item, which sub-line (for paragraphs).
        // Max visual rows = MAX_MENU_ITEMS * max_paragraph_lines. Cap generously.
        struct VisRow {
            uint8_t itemIdx;    // logical item index
            uint8_t subLine;    // 0 for normal/single-text, 0..N for paragraph lines
        };
        static const int MAX_VIS_ROWS = 64;
        VisRow vmap[MAX_VIS_ROWS];
        int totalVisRows = 0;
        int cursorVisRow = 0;   // visual row where cursor item starts

        for (uint8_t item = 0; item < m->itemCount && totalVisRows < MAX_VIS_ROWS; item++) {
            if (item == cursorPosition)
                cursorVisRow = totalVisRows;

            if (m->items[item].isTextItem() && m->items[item].textInfo->paragraph) {
                // Pre-compute how many wrapped lines this paragraph needs
                MenuTextInfo* ti = m->items[item].textInfo;
                const char* txt = (ti->textPtr && *ti->textPtr) ? *ti->textPtr : "";
                uint8_t txtLen = (uint8_t)strlen(txt);
                if (txtLen > MAX_TEXT_LENGTH) txtLen = MAX_TEXT_LENGTH;

                const uint8_t* textFont = resolveTextFont(ti);
                d->setFont(textFont);
                uint8_t tfw = dm.fontWidth(textFont);

                uint8_t cpl = ti->maxChars;
                if (cpl == 0 || cpl > MAX_TEXT_LENGTH) {
                    uint8_t indent = (dm.selectionStyle == SELECTION_ARROW)
                                     ? dm.arrowIndentWidth(dm.standardFont) : 2;
                    int16_t usable = (int16_t)dm.screenWidth - indent - 2;
                    cpl = (usable > 0) ? (uint8_t)(usable / tfw) : 1;
                }
                if (cpl < 1) cpl = 1;

                // Count wrapped lines
                uint8_t lines = 0;
                uint8_t pos = 0;
                while (pos < txtLen && totalVisRows + lines < MAX_VIS_ROWS) {
                    uint8_t lineLen = 0;
                    uint8_t lastSpace = 0;
                    while (lineLen < cpl && (pos + lineLen) < txtLen) {
                        char ch = txt[pos + lineLen];
                        if (ch == ' ' || ch == '-') lastSpace = lineLen + 1;
                        lineLen++;
                    }
                    if ((pos + lineLen) < txtLen && lineLen >= cpl && lastSpace > 0)
                        lineLen = lastSpace;
                    vmap[totalVisRows + lines] = { item, lines };
                    lines++;
                    pos += lineLen;
                    while (pos < txtLen && txt[pos] == ' ') pos++;
                }
                if (lines == 0) {
                    vmap[totalVisRows] = { item, 0 };
                    lines = 1;
                }
                ti->wrappedLines = lines;
                totalVisRows += lines;
                d->setFont(dm.standardFont);
            } else {
                // Normal item or single-line text: 1 visual row
                vmap[totalVisRows] = { item, 0 };
                totalVisRows++;
                if (m->items[item].isTextItem())
                    m->items[item].textInfo->wrappedLines = 1;
            }
        }

        // ── Scroll: ensure cursor's visual row is visible ─────────────────────
        int dispStartRow = 0;
        if (cursorVisRow >= visItems)
            dispStartRow = cursorVisRow - visItems + 1;
        if (dispStartRow + visItems > totalVisRows) {
            dispStartRow = totalVisRows - visItems;
            if (dispStartRow < 0) dispStartRow = 0;
        }

        // ── Compute Y base ────────────────────────────────────────────────────
        int16_t yBase;
        if (m->layoutAnchor == BOTTOM) {
            int rowsOnScreen = totalVisRows - dispStartRow;
            if (rowsOnScreen > visItems) rowsOnScreen = visItems;
            yBase = dm.oy() + dm.screenHeight - 2 - (rowsOnScreen - 1) * dm.lineHeight;
        } else {
            yBase = dm.oy() + dm.menuStartY;
        }

        // ── Render visible rows ───────────────────────────────────────────────
        for (int vr = 0; vr < visItems && (dispStartRow + vr) < totalVisRows; vr++) {
            VisRow& row  = vmap[dispStartRow + vr];
            uint8_t idx  = row.itemIdx;
            int16_t yPos = yBase + vr * dm.lineHeight;
            bool    isSel = (idx == cursorPosition);

            // ── TEXT ROW ──────────────────────────────────────────────────────
            if (m->items[idx].isTextItem()) {
                MenuTextInfo* ti = m->items[idx].textInfo;
                const char* txt = (ti->textPtr && *ti->textPtr) ? *ti->textPtr : "";
                uint8_t txtLen = (uint8_t)strlen(txt);
                if (txtLen > MAX_TEXT_LENGTH) txtLen = MAX_TEXT_LENGTH;

                const uint8_t* textFont = resolveTextFont(ti);
                d->setFont(textFont);
                uint8_t tfw = dm.fontWidth(textFont);

                uint8_t cpl = ti->maxChars;
                if (cpl == 0 || cpl > MAX_TEXT_LENGTH) {
                    uint8_t indent = (dm.selectionStyle == SELECTION_ARROW)
                                     ? dm.arrowIndentWidth(dm.standardFont) : 2;
                    int16_t usable = (int16_t)dm.screenWidth - indent - 2;
                    cpl = (usable > 0) ? (uint8_t)(usable / tfw) : 1;
                }
                if (cpl < 1) cpl = 1;

                int16_t drawX;
                if (ti->ofsX >= 0)
                    drawX = dm.ox() + ti->ofsX;
                else if (dm.selectionStyle == SELECTION_ARROW)
                    drawX = dm.ox() + dm.arrowIndentWidth(dm.standardFont);
                else
                    drawX = dm.ox() + 2;

                int16_t drawY = (ti->ofsY >= 0) ? dm.oy() + ti->ofsY : yPos;

                if (ti->paragraph) {
                    // Find the specific sub-line to render for this visual row
                    uint8_t targetLine = row.subLine;
                    uint8_t pos = 0;
                    uint8_t curLine = 0;
                    char lineBuf[MAX_TEXT_LENGTH + 1];

                    while (pos < txtLen) {
                        uint8_t lineLen = 0;
                        uint8_t lastSpace = 0;
                        while (lineLen < cpl && (pos + lineLen) < txtLen) {
                            char ch = txt[pos + lineLen];
                            if (ch == ' ' || ch == '-') lastSpace = lineLen + 1;
                            lineLen++;
                        }
                        if ((pos + lineLen) < txtLen && lineLen >= cpl && lastSpace > 0)
                            lineLen = lastSpace;

                        if (curLine == targetLine) {
                            if (lineLen > MAX_TEXT_LENGTH) lineLen = MAX_TEXT_LENGTH;
                            memcpy(lineBuf, txt + pos, lineLen);
                            while (lineLen > 0 && lineBuf[lineLen - 1] == ' ') lineLen--;
                            lineBuf[lineLen] = '\0';
                            d->setCursor(drawX, drawY);
                            d->print(lineBuf);
                            break;
                        }
                        pos += lineLen;
                        while (pos < txtLen && txt[pos] == ' ') pos++;
                        curLine++;
                    }
                } else {
                    // Single-line text, auto-scroll if too long
                    if (txtLen <= cpl) {
                        d->setCursor(drawX, drawY);
                        d->print(txt);
                    } else {
                        unsigned long now = millis();
                        int scrollRange = txtLen - cpl + 1;
                        int sp = (int)((now / 400) % (scrollRange * 2));
                        if (sp >= scrollRange) sp = scrollRange * 2 - sp - 1;
                        if (sp < 0) sp = 0;
                        d->setCursor(drawX, drawY);
                        for (uint8_t c = 0; c < cpl && (sp + c) < txtLen; c++)
                            d->print(txt[sp + c]);
                    }
                }
                d->setFont(dm.standardFont);
                continue;
            }

            // ── NORMAL ITEM ───────────────────────────────────────────────────

            // ── Format value summary FIRST (needed to compute name space) ─────
            char valBuf[20];
            const char* valUnit = "";
            int16_t valPixelW = 0;
            bool hasValue = false;

            // On very narrow screens (width <= 36), skip the value entirely
            // so the name has full width and can auto-scroll
            bool hideValueForWidth = (dm.screenWidth <= 36);

            if (m->items[idx].valueAdjuster && !hideValueForWidth) {
                hasValue = true;
                ValueAdjuster* adj = m->items[idx].valueAdjuster;
                if (adj->getType() == ValueAdjuster::TYPE_DATE) {
                    static_cast<DateAdjuster*>(adj)->formatSummary(valBuf, sizeof(valBuf));
                } else if (adj->getType() == ValueAdjuster::TYPE_TIME) {
                    static_cast<TimeAdjuster*>(adj)->formatSummary(valBuf, sizeof(valBuf));
                } else if (adj->getType() == ValueAdjuster::TYPE_GAUGE) {
                    static_cast<DisplayGaugeBase*>(adj)->formatSummary(valBuf, sizeof(valBuf));
                } else if (adj->getType() == ValueAdjuster::TYPE_BAR) {
                    static_cast<DisplayBarBase*>(adj)->formatSummary(valBuf, sizeof(valBuf));
                } else if (adj->getType() == ValueAdjuster::TYPE_MULTI) {
                    MultiSelectAdjuster* ms = static_cast<MultiSelectAdjuster*>(adj);
                    ms->formatSummary(valBuf, sizeof(valBuf));
                } else if (adj->getType() == ValueAdjuster::TYPE_STRING) {
                    static_cast<StringAdjuster*>(adj)->formatSummary(valBuf, sizeof(valBuf));
                } else {
                    int dec = adj->getDecimalPlaces();
                    if (dec > 0) { char fmt[16]; snprintf(fmt,sizeof(fmt),"%%.%df",dec); snprintf(valBuf,sizeof(valBuf),fmt,adj->getValue()); }
                    else         { snprintf(valBuf, sizeof(valBuf), "%.0f", adj->getValue()); }
                }
                valUnit = (adj->getType() == ValueAdjuster::TYPE_MULTI ||
                           adj->getType() == ValueAdjuster::TYPE_BAR ||
                           adj->getType() == ValueAdjuster::TYPE_STRING) ? "" : adj->getUnit();
                // Total pixel width of value + unit + 1 char gap
                // Use getStrWidth() for accurate measurement with proportional fonts
                {
                    char fullVal[40];
                    snprintf(fullVal, sizeof(fullVal), " %s%s", valBuf, valUnit);
                    valPixelW = (int16_t)d->getStrWidth(fullVal);
                }
            }

            // ── Compute effective max chars for the name ──────────────────────
            // Start with user-set maxChars (0 = no explicit limit)
            int maxCh = m->layoutMaxChars;

            // Always calculate the auto-fit limit from available screen pixels
            // This enables auto-scroll for names that don't fit even without
            // an explicit setMenuLayout() call.
            {
                uint8_t indent = (dm.selectionStyle == SELECTION_ARROW) ? dm.arrowIndentWidth(dm.standardFont) : 2;
                int16_t availPx = dm.screenWidth - indent - (hasValue ? valPixelW : 0);
                // Reserve scrollbar width if visible
                if (dm.showScrollBar && m->layoutVisibleItems <= 0)
                    availPx -= dm.scrollIndicatorWidth + 1;
                int autoMax = (availPx > 0 && fw > 0) ? (int)(availPx / fw) : 1;
                if (autoMax < 1) autoMax = 1;
                // Use the smaller of user limit and auto limit
                if (maxCh <= 0 || autoMax < maxCh)
                    maxCh = autoMax;
            }

            // ── Prepare display name (truncate / scroll) ──────────────────────
            const char* fullName = m->items[idx].name;
            int         nameLen  = (int)strlen(fullName);
            char        dispName[MAX_MENU_NAME_LENGTH + 1];

            if (maxCh > 0 && nameLen > maxCh) {
                if (isSel) {
                    // Selected + overflows: auto-scroll
                    if (scrollCursorItem != idx) {
                        scrollCursorItem = idx;
                        scrollOffset     = 0;
                        scrollLastUpdate = millis();
                        scrollPauseUntil = millis() + 800;
                    }
                    unsigned long now = millis();
                    if (now >= scrollPauseUntil && now - scrollLastUpdate >= 350) {
                        scrollLastUpdate = now;
                        scrollOffset++;
                        if (scrollOffset > nameLen - maxCh) {
                            scrollOffset     = 0;
                            scrollPauseUntil = now + 800;
                        }
                    }
                    strncpy(dispName, fullName + scrollOffset, maxCh);
                    dispName[maxCh] = '\0';
                } else {
                    strncpy(dispName, fullName, maxCh);
                    dispName[maxCh] = '\0';
                }
            } else {
                strncpy(dispName, fullName, MAX_MENU_NAME_LENGTH);
                dispName[MAX_MENU_NAME_LENGTH] = '\0';
            }

            // ── Draw selection indicator + name ───────────────────────────────
            switch (dm.selectionStyle) {
                case SELECTION_INVERSE:
                    if (isSel) dm.drawSelectionBox(yPos, fontH);
                    d->setCursor(dm.ox() + 2, yPos);
                    d->print(dispName);
                    break;

                case SELECTION_BRACKET:
                    d->setCursor(dm.ox(), yPos);
                    if (isSel) {
                        d->print("["); d->print(dispName); d->print("]");
                    } else {
                        d->print(dispName);
                    }
                    break;

                case SELECTION_NONE:
                    d->setCursor(dm.ox(), yPos);
                    d->print(dispName);
                    break;

                case SELECTION_BLINK:
                    if (!isSel || dm.isBlinkVisible()) {
                        d->setCursor(dm.ox(), yPos);
                        d->print(dispName);
                    }
                    break;

                case SELECTION_ARROW:
                default: {
                    uint8_t arrowIndent = dm.arrowIndentWidth(dm.standardFont);
                    if (isSel) { d->setCursor(dm.ox(),                yPos); d->print("> "); }
                    else        { d->setCursor(dm.ox() + arrowIndent, yPos); }
                    d->print(dispName);
                    break;
                }
            }

            // ── Draw value summary on the right ───────────────────────────────
            if (hasValue) {
                // For blink: hide value too when selected item blinks off
                if (dm.selectionStyle != SELECTION_BLINK || !isSel || dm.isBlinkVisible()) {
                    // Build full value string
                    char fullValStr[40];
                    snprintf(fullValStr, sizeof(fullValStr), "%s%s", valBuf, valUnit);
                    int16_t fullValW = (int16_t)d->getStrWidth(fullValStr);

                    // Calculate available space for value (after name + gap)
                    uint8_t indent = (dm.selectionStyle == SELECTION_ARROW) ? dm.arrowIndentWidth(dm.standardFont) : 2;
                    int16_t namePixW = (int16_t)d->getStrWidth(dispName);
                    int16_t valAvail = dm.screenWidth - indent - namePixW - fw;

                    if (fullValW > valAvail && valAvail > 0 && isSel) {
                        // Value too wide for selected row — scroll it
                        int scrollRange = (int)strlen(fullValStr) - (int)(valAvail / fw);
                        if (scrollRange > 0) {
                            unsigned long now = millis();
                            int scrollPos = (int)((now / 400) % ((scrollRange + 1) * 2));
                            if (scrollPos > scrollRange) scrollPos = (scrollRange + 1) * 2 - scrollPos - 1;
                            if (scrollPos < 0) scrollPos = 0;
                            int16_t xPos = dm.ox() + dm.screenWidth - valAvail;
                            d->setCursor(xPos, yPos);
                            int maxC = (int)(valAvail / fw);
                            for (int ci = 0; ci < maxC && (scrollPos + ci) < (int)strlen(fullValStr); ci++) {
                                d->print(fullValStr[scrollPos + ci]);
                            }
                        } else {
                            int16_t xPos = dm.ox() + dm.screenWidth - valPixelW;
                            d->setCursor(xPos, yPos);
                            d->print(fullValStr);
                        }
                    } else {
                        int16_t xPos = dm.ox() + dm.screenWidth - valPixelW;
                        d->setCursor(xPos, yPos);
                        d->print(fullValStr);
                    }
                }
            }

            // Restore normal draw mode after the entire row
            if (dm.selectionStyle == SELECTION_INVERSE && isSel)
                dm.resetDrawColor();
        }

        // Scroll bar (only when using default or maxVisibleItems layout)
        if (m->layoutVisibleItems <= 0)
            dm.drawScrollBar(cursorVisRow, totalVisRows, visItems);

        // ── Page counter in title bar ─────────────────────────────────────────
        // Shown when layoutVisibleItems is set and showCounter is true
        if (m->layoutVisibleItems > 0 && m->layoutShowCounter && m->itemCount > visItems) {
            d->setFont(dm.titleFont);
            char pageBuf[24];  // large enough for INT_MAX/INT_MAX
            snprintf(pageBuf, sizeof(pageBuf), "%d/%d", cursorPosition + 1, m->itemCount);
            int16_t pgW = (int16_t)d->getStrWidth(pageBuf);
            int16_t pgX = dm.ox() + dm.screenWidth - pgW - 1;
            int16_t pgY = dm.oy() + dm.titleHeight - 2;
            d->setCursor(pgX, pgY);
            d->print(pageBuf);
            d->setFont(dm.standardFont);
        }

        // Screen-info overlay
        if (m->hasScreenInfo && m->screenInfoCallback) m->screenInfoCallback();

        // One-button hint
        if (inputMode == INPUT_ONE_BUTTON) {
            OneButtonInputManager* ob = static_cast<OneButtonInputManager*>(inputMgr);
            int16_t hintY = dm.oy() + dm.screenHeight - 2;
            d->setCursor(dm.ox(), hintY);
            if (dm.screenWidth >= 96) {
                d->print("Short=");
                d->print(ob->shortPressIsUp ? "Up" : "Down");
                d->print(" Long=Select");
            } else {
                d->print(ob->shortPressIsUp ? "S=Up" : "S=Dn");
                d->print(" L=Ok");
            }
        }

        d->sendBuffer();
    }
};

// Backward-compatible alias so existing code still compiles unchanged
using ESP32_MenuSystem = MenuSystem;

#endif // MENU_SYSTEM_H
