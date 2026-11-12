// Adjusters.h
// Pure-data value adjuster classes.
// No display or input dependencies — can be unit-tested on any platform.
#ifndef ADJUSTERS_H
#define ADJUSTERS_H

#include <Arduino.h>
#include "MenuTypes.h"

// -----------------------------------------------------------------------------
// ValueAdjuster — abstract base
// -----------------------------------------------------------------------------
class ValueAdjuster {
public:
    enum AdjusterType {
        TYPE_FLOAT = 0,
        TYPE_INT   = 1,
        TYPE_BOOL  = 2,
        TYPE_MULTI = 3,
        TYPE_DATE  = 4,
        TYPE_TIME  = 5,
        TYPE_GAUGE = 6,
        TYPE_BAR   = 7,
        TYPE_STRING = 8
    };

    virtual float        getValue()        = 0;
    virtual void         setValue(float v) = 0;
    virtual float        getIncrement()    = 0;
    virtual float        getMin()          = 0;
    virtual float        getMax()          = 0;
    virtual const char*  getUnit()         = 0;
    virtual int          getDecimalPlaces()= 0;
    virtual AdjusterType getType() const   = 0;
    virtual ~ValueAdjuster() {}
};

// -----------------------------------------------------------------------------
// FloatValueAdjuster
// -----------------------------------------------------------------------------
class FloatValueAdjuster : public ValueAdjuster {
private:
    float*      valuePtr;
    float       increment;
    float       minValue;
    float       maxValue;
    int         decimalPlaces;
    const char* unit;
    bool        wrapAround;

public:
    FloatValueAdjuster(float* value, float inc, float min, float max,
                       int decimals = 1, const char* valueUnit = "",
                       bool wrap = false)
        : valuePtr(value), increment(inc), minValue(min), maxValue(max),
          decimalPlaces(decimals), unit(valueUnit), wrapAround(wrap) {}

    AdjusterType getType() const override { return TYPE_FLOAT; }
    float getValue() override { return *valuePtr; }

    void setValue(float v) override {
        if (wrapAround) {
            if (v > maxValue) v = minValue;
            else if (v < minValue) v = maxValue;
        } else {
            if (v < minValue) v = minValue;
            if (v > maxValue) v = maxValue;
        }
        *valuePtr = v;
    }

    float       getIncrement()    override { return increment; }
    float       getMin()          override { return minValue; }
    float       getMax()          override { return maxValue; }
    const char* getUnit()         override { return unit; }
    int         getDecimalPlaces()override { return decimalPlaces; }
};

// -----------------------------------------------------------------------------
// IntValueAdjuster
// -----------------------------------------------------------------------------
class IntValueAdjuster : public ValueAdjuster {
private:
    int*        valuePtr;
    int         increment;
    int         minValue;
    int         maxValue;
    const char* unit;
    bool        wrapAround;

public:
    IntValueAdjuster(int* value, int inc, int min, int max,
                     const char* valueUnit = "", bool wrap = false)
        : valuePtr(value), increment(inc), minValue(min), maxValue(max),
          unit(valueUnit), wrapAround(wrap) {}

    AdjusterType getType() const override { return TYPE_INT; }
    float getValue() override { return static_cast<float>(*valuePtr); }

    void setValue(float v) override {
        int iv = static_cast<int>(v);
        if (wrapAround) {
            if (iv > maxValue) iv = minValue;
            else if (iv < minValue) iv = maxValue;
        } else {
            if (iv < minValue) iv = minValue;
            if (iv > maxValue) iv = maxValue;
        }
        *valuePtr = iv;
    }

    float       getIncrement()    override { return static_cast<float>(increment); }
    float       getMin()          override { return static_cast<float>(minValue); }
    float       getMax()          override { return static_cast<float>(maxValue); }
    const char* getUnit()         override { return unit; }
    int         getDecimalPlaces()override { return 0; }
};

// -----------------------------------------------------------------------------
// BoolValueAdjuster
// -----------------------------------------------------------------------------
class BoolValueAdjuster : public ValueAdjuster {
private:
    bool* valuePtr;
    bool  tempValue;
    char  trueLabel [MAX_BOOL_LABEL_LENGTH];
    char  falseLabel[MAX_BOOL_LABEL_LENGTH];

public:
    BoolValueAdjuster(bool* value,
                      const char* trueText  = "On",
                      const char* falseText = "Off")
        : valuePtr(value), tempValue(*value) {
        strncpy(trueLabel,  trueText,  MAX_BOOL_LABEL_LENGTH - 1);
        strncpy(falseLabel, falseText, MAX_BOOL_LABEL_LENGTH - 1);
        trueLabel [MAX_BOOL_LABEL_LENGTH - 1] = '\0';
        falseLabel[MAX_BOOL_LABEL_LENGTH - 1] = '\0';
    }

    AdjusterType getType() const override { return TYPE_BOOL; }
    float getValue() override { return *valuePtr ? 1.0f : 0.0f; }
    void  setValue(float) override { *valuePtr = !(*valuePtr); }

    float       getIncrement()    override { return 1.0f; }
    float       getMin()          override { return 0.0f; }
    float       getMax()          override { return 1.0f; }
    const char* getUnit()         override { return ""; }
    int         getDecimalPlaces()override { return 0; }

    // Bool-specific
    const char* getTrueLabel()    const { return trueLabel; }
    const char* getFalseLabel()   const { return falseLabel; }
    const char* getCurrentLabel() const { return *valuePtr ? trueLabel : falseLabel; }
    bool        isTrue()          const { return *valuePtr; }

    void        setTempValue(bool v)    { tempValue = v; }
    bool        getTempValue()    const { return tempValue; }
    void        applyTempValue()        { *valuePtr = tempValue; }
    const char* getTempLabel()    const { return tempValue ? trueLabel : falseLabel; }
};

#if MENU_ENABLE_MULTISELECT
// -----------------------------------------------------------------------------
// MultiSelectAdjuster
//
// ── Overview ────────────────────────────────────────────────────────────────
//   Displays a scrollable checklist. The user ticks/unticks options, then
//   presses OK to confirm. Min/max selection limits are enforced on OK.
//
// ── Hidden mode ─────────────────────────────────────────────────────────────
//   When hideEmpty=true, slots whose label is "" (empty string) or that were
//   never set are hidden from the scroll list. This is ideal for dynamic
//   lists like WiFi scan results where you allocate a capacity of 16 but
//   only populate 5 after scanning.
//
//   When all visible options are 0, the widget shows "[None]" and only OK.
//
// ── Min / max selection ─────────────────────────────────────────────────────
//   minSelect: minimum number of ticked options required to press OK.
//              0 = no minimum (default).
//   maxSelect: maximum number of ticked options allowed.
//              0 = no limit (default). Once the limit is reached, further
//              toggles on un-selected items are blocked.
//
// ── Usage examples ──────────────────────────────────────────────────────────
//
//   // === Static list (days of week) ===
//   // Must select 2..7 days. All 7 slots visible.
//   MultiSelectAdjuster days("Days", 7, 2, 7);
//   days.setOption(0, "Mon");
//   days.setOption(1, "Tue");
//   days.setOption(2, "Wed");
//   days.setOption(3, "Thu");
//   days.setOption(4, "Fri", true);   // pre-selected
//   days.setOption(5, "Sat", true);   // pre-selected
//   days.setOption(6, "Sun");
//   menu.addMultiSelectMenuItem(menuId, "Work Days", &days);
//
//   // === Dynamic list (WiFi scan) ===
//   // Allocate 16 slots, hidden mode, select exactly 1.
//   MultiSelectAdjuster wifi("WiFi", 16, 0, 1, true);
//   // After scanning:
//   wifi.clearOptions();
//   for (int i = 0; i < found && i < 16; i++)
//       wifi.setOption(i, ssidList[i]);
//   // Now only `found` options are visible; the rest are hidden.
//   // If found==0, widget shows [None].
//
//   // === Check results ===
//   for (uint8_t i = 0; i < wifi.getCapacity(); i++) {
//       if (wifi.isSelected(i))
//           Serial.println(wifi.getLabel(i));
//   }
//
// ── Legacy addOption() ──────────────────────────────────────────────────────
//   The old addOption() still works for simple sequential building:
//     MultiSelectAdjuster ms;
//     ms.addOption("Alpha");
//     ms.addOption("Beta", true);
//   This auto-increments an internal counter just like before.
// =============================================================================
class MultiSelectAdjuster : public ValueAdjuster {
public:
    // ── Constants ─────────────────────────────────────────────────────────────
    static const uint8_t HARD_CAP = MAX_MULTI_SELECT_OPTIONS;

private:
    const char* labels[HARD_CAP];      // pointers to label strings (caller-owned)
    bool        selected[HARD_CAP];    // tick state per slot
    uint8_t     capacity;              // how many slots are allocated (1..HARD_CAP)
    uint8_t     addCount;              // auto-increment counter for addOption()
    uint8_t     minSelect;             // min ticked items to allow OK (0=none)
    uint8_t     maxSelect;             // max ticked items allowed    (0=unlimited)
    bool        hideEmpty;             // true = hide slots with nullptr/empty label

    uint8_t     cursorIndex;           // cursor in the VISIBLE index space
    // Visible list: maps visible row index → real slot index.
    // Rebuilt whenever the list changes. Last entry is the OK row sentinel (0xFF).
    uint8_t     visMap[HARD_CAP + 1];  // +1 for OK row
    uint8_t     visCount;              // number of visible option rows (excl. OK)

public:
    // ── Constructors ──────────────────────────────────────────────────────────

    // Full constructor
    //   name      – not stored here; passed to addMultiSelectMenuItem as the menu label
    //   cap       – number of option slots to allocate (capped to HARD_CAP)
    //   minSel    – minimum selected items to allow OK (0 = no minimum)
    //   maxSel    – maximum selected items allowed     (0 = unlimited)
    //   hidden    – true = hide empty/null label slots
    MultiSelectAdjuster(uint8_t cap = 0,
                        uint8_t minSel = 0, uint8_t maxSel = 0,
                        bool hidden = false)
        : capacity(cap > HARD_CAP ? HARD_CAP : cap),
          addCount(0),
          minSelect(minSel), maxSelect(maxSel),
          hideEmpty(hidden),
          cursorIndex(0), visCount(0)
    {
        memset(selected, 0, sizeof(selected));
        memset(labels,   0, sizeof(labels));
        rebuildVisMap();
    }

    // ── Option management ─────────────────────────────────────────────────────

    // Set a specific slot by index. Grows capacity if needed.
    void setOption(uint8_t idx, const char* label, bool state = false) {
        if (idx >= HARD_CAP) return;
        if (idx >= capacity) capacity = idx + 1;
        labels[idx]   = label;
        selected[idx] = state;
        rebuildVisMap();
    }

    // Sequential append (legacy compatible). Returns slot index, or -1 if full.
    int addOption(const char* label, bool state = false) {
        if (addCount >= HARD_CAP) return -1;
        uint8_t idx = addCount++;
        if (idx >= capacity) capacity = idx + 1;
        labels[idx]   = label;
        selected[idx] = state;
        rebuildVisMap();
        return (int)idx;
    }

    // Clear all slots and reset. Use before re-populating (e.g. WiFi re-scan).
    void clearOptions() {
        memset(selected, 0, sizeof(selected));
        memset(labels,   0, sizeof(labels));
        addCount    = 0;
        cursorIndex = 0;
        rebuildVisMap();
    }

    // Reset only selection states (keep labels).
    void clearSelections() {
        memset(selected, 0, sizeof(selected));
    }

    // Change limits at runtime (e.g. after scan sets new capacity).
    void setLimits(uint8_t minSel, uint8_t maxSel) {
        minSelect = minSel;
        maxSelect = maxSel;
    }

    void setCapacity(uint8_t cap) {
        capacity = (cap > HARD_CAP) ? HARD_CAP : cap;
        rebuildVisMap();
    }

    void setHideEmpty(bool hide) {
        hideEmpty = hide;
        rebuildVisMap();
    }

    // ── ValueAdjuster interface stubs ─────────────────────────────────────────
    AdjusterType getType() const override { return TYPE_MULTI; }
    float        getValue()        override { return 0; }
    void         setValue(float)   override {}
    float        getIncrement()    override { return 1; }
    float        getMin()          override { return 0; }
    float        getMax()          override { return (float)capacity; }
    const char*  getUnit()         override { return ""; }
    int          getDecimalPlaces()override { return 0; }

    // ── Navigation (called by MenuSystem / WidgetRenderer) ────────────────────

    uint8_t getCursorIndex()  const { return cursorIndex; }
    uint8_t getCapacity()     const { return capacity; }

    // Visible option count (excludes hidden empty slots, excludes OK row).
    uint8_t getVisibleCount() const { return visCount; }

    // Total rows in the scroll list (visible options + OK row).
    uint8_t getTotalRows()    const { return visCount + 1; }

    // True when cursor is on the OK row.
    bool isOnOk() const { return cursorIndex >= visCount; }

    // True when there are zero visible options.
    bool isEmpty() const { return visCount == 0; }

    // Get the real slot index for a visible row. Returns 0xFF for OK row.
    uint8_t realIndex(uint8_t visIdx) const {
        return (visIdx < visCount) ? visMap[visIdx] : 0xFF;
    }

    // Is the slot at visible index `visIdx` selected?
    bool isVisibleSelected(uint8_t visIdx) const {
        uint8_t ri = realIndex(visIdx);
        return (ri < HARD_CAP) ? selected[ri] : false;
    }

    // Get label for visible row index.
    const char* getVisibleLabel(uint8_t visIdx) const {
        uint8_t ri = realIndex(visIdx);
        return (ri < HARD_CAP && labels[ri]) ? labels[ri] : "";
    }

    // ── Direct slot access (by real index) ────────────────────────────────────

    bool isSelected(uint8_t realIdx) const {
        return (realIdx < capacity) ? selected[realIdx] : false;
    }

    const char* getLabel(uint8_t realIdx) const {
        return (realIdx < capacity && labels[realIdx]) ? labels[realIdx] : "";
    }

    void setSelected(uint8_t realIdx, bool state) {
        if (realIdx < capacity) selected[realIdx] = state;
    }

    // ── Cursor movement ───────────────────────────────────────────────────────
    void moveCursorUp() {
        if (cursorIndex > 0) cursorIndex--;
        else cursorIndex = visCount;  // wrap to OK
    }

    void moveCursorDown() {
        if (cursorIndex < visCount) cursorIndex++;
        else cursorIndex = 0;         // wrap to top
    }

    // ── Toggle current option ─────────────────────────────────────────────────
    // Respects maxSelect: blocks selecting more than max.
    void toggleCurrent() {
        if (cursorIndex >= visCount) return;  // on OK row
        uint8_t ri = visMap[cursorIndex];
        if (ri >= HARD_CAP) return;

        if (selected[ri]) {
            // Always allow un-selecting
            selected[ri] = false;
        } else {
            // Check max limit before selecting
            if (maxSelect > 0 && getSelectedCount() >= maxSelect) return;
            selected[ri] = true;
        }
    }

    // ── OK validation ─────────────────────────────────────────────────────────
    // Returns true if selection count meets min requirement.
    // If empty (no visible options), OK is always allowed.
    bool canConfirm() const {
        if (visCount == 0) return true;  // [None] state — always allow exit
        return getSelectedCount() >= minSelect;
    }

    // ── Result queries ────────────────────────────────────────────────────────

    bool getResult(uint8_t realIdx) const { return isSelected(realIdx); }

    uint8_t getSelectedCount() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < capacity; i++) {
            if (selected[i] && isSlotVisible(i)) n++;
        }
        return n;
    }

    // Alias for backward compatibility
    uint8_t getOptionCount() const { return visCount; }

    // ── Summary for menu list ─────────────────────────────────────────────────
    void formatSummary(char* buf, uint8_t sz) const {
        if (visCount == 0) {
            snprintf(buf, sz, "None");
        } else {
            snprintf(buf, sz, "%d/%d", getSelectedCount(), visCount);
        }
    }

    uint8_t getMinSelect() const { return minSelect; }
    uint8_t getMaxSelect() const { return maxSelect; }

private:
    // Is a real slot visible? (non-null label, or hideEmpty is off)
    bool isSlotVisible(uint8_t realIdx) const {
        if (realIdx >= capacity) return false;
        if (!hideEmpty) return true;
        return labels[realIdx] != nullptr && labels[realIdx][0] != '\0';
    }

    // Rebuild the visible index map. Call after any label/capacity change.
    void rebuildVisMap() {
        visCount = 0;
        for (uint8_t i = 0; i < capacity && visCount < HARD_CAP; i++) {
            if (isSlotVisible(i)) {
                visMap[visCount++] = i;
            }
        }
        visMap[visCount] = 0xFF;  // OK row sentinel
        // Clamp cursor
        if (cursorIndex > visCount) cursorIndex = visCount;
    }
};

#endif // MENU_ENABLE_MULTISELECT

#endif // ADJUSTERS_H
