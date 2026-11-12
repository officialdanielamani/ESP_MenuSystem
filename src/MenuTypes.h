#ifndef MENU_TYPES_H
#define MENU_TYPES_H
#include <Arduino.h>
#include "MenuConfig.h"   // tunable limits + feature switches

// ── Selection cursor style ────────────────────────────────────────────────────
enum SelectionStyle : uint8_t {
    SELECTION_ARROW   = 0,
    SELECTION_INVERSE = 1,
    SELECTION_BRACKET = 2,
    SELECTION_NONE    = 3,
    SELECTION_BLINK   = 4
};

// ── Menu item anchor position ────────────────────────────────────────────────
enum MenuAnchor : uint8_t { TOP = 0, BOTTOM = 1 };

// ── Text font selector ──────────────────────────────────────────────────────
// TEXT_FONT_DEFAULT uses standardFont; the named presets pick a fixed font;
// TEXT_FONT_CUSTOM lets the user supply an arbitrary U8g2 font pointer.
enum TextFontType : uint8_t {
    TEXT_FONT_DEFAULT = 0,
    TEXT_FONT_SMALL   = 1,
    TEXT_FONT_LARGE   = 2,
    TEXT_FONT_CUSTOM  = 3
};

typedef void (*ScreenInfoCallback)();
typedef void (*SimpleMenuFunction)();

class MenuCallback {
public:
    virtual void execute() = 0;
    virtual ~MenuCallback() {}
};

class FunctionCallback : public MenuCallback {
    SimpleMenuFunction fn;
public:
    FunctionCallback(SimpleMenuFunction f) : fn(f) {}
    void execute() override { if (fn) fn(); }
};

class ValueAdjuster;

// ── MenuTextInfo ─────────────────────────────────────────────────────────────
// Descriptor for a non-selectable text row embedded in a menu.
// The text source is a const char** (pointer-to-pointer) so it can track
// a changing String.c_str() or a static literal without copying.
//
// paragraph  : true = word-wrap across multiple visual rows
//              false = single row, auto-scroll if wider than maxChars
// fontType   : TEXT_FONT_DEFAULT / SMALL / LARGE / CUSTOM
// customFont : only used when fontType == TEXT_FONT_CUSTOM
// maxChars   : max characters per visual line (0 = auto from screen width)
// ofsX, ofsY : pixel offset overrides (-1 = use auto position)
struct MenuTextInfo {
    const char**    textPtr;      // points to the caller's const char* variable
    bool            paragraph;
    TextFontType    fontType;
    const uint8_t*  customFont;
    uint8_t         maxChars;     // per-line char limit (0=auto)
    int8_t          ofsX;         // -1 = auto
    int8_t          ofsY;         // -1 = auto
    uint8_t         wrappedLines; // computed at render time: how many visual rows

    MenuTextInfo()
        : textPtr(nullptr), paragraph(false),
          fontType(TEXT_FONT_DEFAULT), customFont(nullptr),
          maxChars(0), ofsX(-1), ofsY(-1), wrappedLines(1) {}
};

// ── MenuItem ─────────────────────────────────────────────────────────────────
class MenuItem {
public:
    char            name[MAX_MENU_NAME_LENGTH];
    int8_t          nextMenuId;
    MenuCallback*   callback;
    ValueAdjuster*  valueAdjuster;
#if MENU_ENABLE_TEXT
    MenuTextInfo*   textInfo;       // non-null → this is a text row (non-selectable)
#endif

    MenuItem()
        : nextMenuId(-1), callback(nullptr), valueAdjuster(nullptr)
#if MENU_ENABLE_TEXT
          , textInfo(nullptr)
#endif
    { name[0] = '\0'; }

    MenuItem(const char* n, int8_t id = -1, MenuCallback* cb = nullptr,
             ValueAdjuster* adj = nullptr, MenuTextInfo* ti = nullptr)
        : nextMenuId(id), callback(cb), valueAdjuster(adj)
#if MENU_ENABLE_TEXT
          , textInfo(ti)
#endif
    {
        (void)ti;
        if (n) {
            strncpy(name, n, MAX_MENU_NAME_LENGTH - 1);
            name[MAX_MENU_NAME_LENGTH - 1] = '\0';
        } else {
            name[0] = '\0';
        }
    }

#if MENU_ENABLE_TEXT
    bool isTextItem()    const { return textInfo != nullptr; }
    bool isSelectable()  const { return textInfo == nullptr; }
#else
    bool isTextItem()    const { return false; }
    bool isSelectable()  const { return true; }
#endif
};

// ── Menu ─────────────────────────────────────────────────────────────────────
class Menu {
public:
    char               title[MAX_MENU_NAME_LENGTH];
    MenuItem           items[MAX_MENU_ITEMS];
    uint8_t            itemCount;
    int8_t             id;
    ScreenInfoCallback screenInfoCallback;
    bool               hasScreenInfo;
    uint8_t            maxVisibleItems;

    uint8_t    layoutVisibleItems;
    uint8_t    layoutMaxChars;
    MenuAnchor layoutAnchor;
    bool       layoutShowCounter;

    Menu()
        : itemCount(0), id(-1), screenInfoCallback(nullptr), hasScreenInfo(false),
          maxVisibleItems(0), layoutVisibleItems(0), layoutMaxChars(0),
          layoutAnchor(TOP), layoutShowCounter(true)
    { title[0] = '\0'; }

    Menu(const char* t, int8_t mid)
        : itemCount(0), id(mid), screenInfoCallback(nullptr), hasScreenInfo(false),
          maxVisibleItems(0), layoutVisibleItems(0), layoutMaxChars(0),
          layoutAnchor(TOP), layoutShowCounter(true)
    {
        if (t) { strncpy(title, t, MAX_MENU_NAME_LENGTH - 1); title[MAX_MENU_NAME_LENGTH - 1] = '\0'; }
        else   { title[0] = '\0'; }
    }

    void addItem(const char* n, int8_t nmid = -1, MenuCallback* cb = nullptr,
                 ValueAdjuster* adj = nullptr, MenuTextInfo* ti = nullptr)
    {
        if (itemCount < MAX_MENU_ITEMS)
            items[itemCount++] = MenuItem(n, nmid, cb, adj, ti);
    }

    void setMaxVisibleItems(uint8_t m) { maxVisibleItems = m; }

    void setLayout(uint8_t maxChars = 0, uint8_t visItems = 0,
                   MenuAnchor anchor = TOP, bool showCounter = true)
    {
        layoutMaxChars = maxChars; layoutVisibleItems = visItems;
        layoutAnchor = anchor; layoutShowCounter = showCounter;
    }

    void setScreenInfoCallback(ScreenInfoCallback cb) {
        screenInfoCallback = cb; hasScreenInfo = (cb != nullptr);
    }

    // Count the total visual rows (text paragraphs expand into multiple rows)
    int totalVisualRows() const {
#if MENU_ENABLE_TEXT
        int rows = 0;
        for (uint8_t i = 0; i < itemCount; i++) {
            if (items[i].textInfo && items[i].textInfo->paragraph)
                rows += items[i].textInfo->wrappedLines;
            else
                rows += 1;
        }
        return rows;
#else
        return itemCount;
#endif
    }
};

#endif
