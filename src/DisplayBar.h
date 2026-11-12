// DisplayBar.h
// Read-only horizontal bar graph items for the ESP32 MenuSystem.
//
// Two concrete types share a common base (DisplayBarBase):
//   DisplayBarInt    — integer sensor values
//   DisplayBarFloat  — float sensor values with configurable decimal places
//
// ── Bar type ────────────────────────────────────────────────────────────────
//   BAR_DEFAULT  : full-screen view with large centred value + bar + min/max
//   BAR_COMPACT  : two-row compact view (name + value on row 1, bar on row 2)
//
// ── Options ─────────────────────────────────────────────────────────────────
//   showMinMax : draw min/max labels at each end of the bar (default true)
//                when disabled in compact mode, the bar extends full width
//   useOver    : show "value/max unit" format  e.g. "35/100 C" (default false)
//                instead of the standard "35C"
//
// ── Usage ───────────────────────────────────────────────────────────────────
//   int   roomTemp = 25;
//   float voltage  = 3.31f;
//
//   // Default (full-screen) bar
//   DisplayBarInt   bTemp(&roomTemp, 0, 50,  "C");
//   DisplayBarInt   bTemp(&roomTemp, 0, 50,  "C", BAR_DEFAULT, true, false);
//
//   // Compact bar, no min/max, with over format
//   DisplayBarInt   bTemp(&roomTemp, 0, 50,  "C", BAR_COMPACT, false, true);
//
//   // Float with 2 decimals, compact
//   DisplayBarFloat bVolt(&voltage,  0.0f, 5.0f, "V", 2, BAR_COMPACT, true, false);
//
//   menu.addDisplayBarInt  (menu1, "Room Temp", &bTemp);
//   menu.addDisplayBarFloat(menu1, "Voltage",   &bVolt);
// =============================================================================
#ifndef DISPLAY_BAR_H
#define DISPLAY_BAR_H

#include <Arduino.h>
#include "Adjusters.h"

// ── Bar display type ─────────────────────────────────────────────────────────
enum BarType {
    BAR_DEFAULT = 0,    // full-screen: title, subtitle, large value, bar, min/max
    BAR_COMPACT = 1     // two-row: name+value row, then bar row
};

// =============================================================================
// DisplayBarBase — shared logic; not used directly
// =============================================================================
class DisplayBarBase : public ValueAdjuster {
public:
    const char* unit;
    BarType     barType;
    bool        showMinMax;
    bool        useOver;      // true = "value/max unit" format

    DisplayBarBase(const char* u, BarType type, bool minmax, bool over)
        : unit(u), barType(type), showMinMax(minmax), useOver(over) {}

    // ── ValueAdjuster stubs (bars are read-only) ──────────────────────────────
    AdjusterType getType()    const override { return TYPE_BAR; }
    void         setValue(float)   override {}
    float        getIncrement()    override { return 1.0f; }
    const char*  getUnit()         override { return unit; }
    int          getDecimalPlaces()override { return 0; }

    // ── Fraction 0.0–1.0 for fill amount ──────────────────────────────────────
    virtual float fraction() const = 0;

    // Format helpers called by WidgetRenderer
    virtual void formatValue  (char* buf, uint8_t sz) const = 0;
    virtual void formatMin    (char* buf, uint8_t sz) const = 0;
    virtual void formatMax    (char* buf, uint8_t sz) const = 0;

    // "value/max unit" or "value unit" depending on useOver
    virtual void formatSummary(char* buf, uint8_t sz) const = 0;

    // "value/max" without unit (used in full-screen display)
    virtual void formatOverValue(char* buf, uint8_t sz) const = 0;
};

// =============================================================================
// DisplayBarInt
// =============================================================================
class DisplayBarInt : public DisplayBarBase {
public:
    const int* valuePtr;
    int        minVal;
    int        maxVal;

    // DisplayBarInt(ptr, min, max, unit, barType, showMinMax, useOver)
    DisplayBarInt(const int*  ptr,
                  int         minV,
                  int         maxV,
                  const char* u       = "",
                  BarType     type    = BAR_DEFAULT,
                  bool        showMM  = true,
                  bool        over    = false)
        : DisplayBarBase(u, type, showMM, over),
          valuePtr(ptr), minVal(minV), maxVal(maxV) {}

    // ── ValueAdjuster interface ───────────────────────────────────────────────
    float getValue() override { return valuePtr ? (float)*valuePtr : 0.0f; }
    float getMin()   override { return (float)minVal; }
    float getMax()   override { return (float)maxVal; }

    int   currentValue() const { return valuePtr ? *valuePtr : 0; }

    float fraction() const override {
        if (maxVal <= minVal) return 0.0f;
        float f = (float)(currentValue() - minVal) / (float)(maxVal - minVal);
        return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    }

    void formatValue(char* buf, uint8_t sz) const override {
        snprintf(buf, sz, "%d", currentValue());
    }
    void formatMin(char* buf, uint8_t sz) const override {
        snprintf(buf, sz, "%d", minVal);
    }
    void formatMax(char* buf, uint8_t sz) const override {
        snprintf(buf, sz, "%d", maxVal);
    }
    void formatOverValue(char* buf, uint8_t sz) const override {
        snprintf(buf, sz, "%d/%d", currentValue(), maxVal);
    }
    void formatSummary(char* buf, uint8_t sz) const override {
        if (useOver)
            snprintf(buf, sz, "%d/%d%s", currentValue(), maxVal, unit);
        else
            snprintf(buf, sz, "%d%s", currentValue(), unit);
    }
};

// =============================================================================
// DisplayBarFloat
// =============================================================================
class DisplayBarFloat : public DisplayBarBase {
public:
    const float* valuePtr;
    float        minVal;
    float        maxVal;
    int          decimals;

    // DisplayBarFloat(ptr, min, max, unit, decimals, barType, showMinMax, useOver)
    DisplayBarFloat(const float* ptr,
                    float        minV,
                    float        maxV,
                    const char*  u       = "",
                    int          dec     = 1,
                    BarType      type    = BAR_DEFAULT,
                    bool         showMM  = true,
                    bool         over    = false)
        : DisplayBarBase(u, type, showMM, over),
          valuePtr(ptr), minVal(minV), maxVal(maxV),
          decimals(dec < 0 ? 0 : dec) {}

    // ── ValueAdjuster interface ───────────────────────────────────────────────
    float getValue() override { return valuePtr ? *valuePtr : 0.0f; }
    float getMin()   override { return minVal; }
    float getMax()   override { return maxVal; }

    float currentValue() const { return valuePtr ? *valuePtr : 0.0f; }

    float fraction() const override {
        if (maxVal <= minVal) return 0.0f;
        float f = (currentValue() - minVal) / (maxVal - minVal);
        return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    }

    void formatValue(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d", (int)currentValue());
        else _fmt(buf, sz, currentValue());
    }
    void formatMin(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d", (int)minVal);
        else _fmt(buf, sz, minVal);
    }
    void formatMax(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d", (int)maxVal);
        else _fmt(buf, sz, maxVal);
    }
    void formatOverValue(char* buf, uint8_t sz) const override {
        char vBuf[12], mBuf[12];
        formatValue(vBuf, sizeof(vBuf));
        formatMax(mBuf, sizeof(mBuf));
        snprintf(buf, sz, "%s/%s", vBuf, mBuf);
    }
    void formatSummary(char* buf, uint8_t sz) const override {
        char vBuf[12];
        formatValue(vBuf, sizeof(vBuf));
        if (useOver) {
            char mBuf[12];
            formatMax(mBuf, sizeof(mBuf));
            snprintf(buf, sz, "%s/%s%s", vBuf, mBuf, unit);
        } else {
            snprintf(buf, sz, "%s%s", vBuf, unit);
        }
    }

private:
    void _fmt(char* buf, uint8_t sz, float v) const {
        char fmt[10];
        snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
        snprintf(buf, sz, fmt, v);
    }
};

#endif // DISPLAY_BAR_H
