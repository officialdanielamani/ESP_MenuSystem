// DisplayGauge.h
// Read-only circular gauge items for the ESP32 MenuSystem.
//
// Two concrete types share a common base (DisplayGaugeBase):
//   DisplayGaugeInt    — integer sensor values
//   DisplayGaugeFloat  — float sensor values with configurable decimal places
//
// Only FILL style is supported: a thick arc filled from start to current value.
// The value is displayed large in the centre of the arc.
// Min label sits at the bottom-left (arc start), max at the bottom-right (arc end).
//
// ── Sweep angle ───────────────────────────────────────────────────────────────
//   Any value 90–355°. Gap always centred at 6 o'clock.
//   270° = classic car gauge,  300° = wide,  180° = half-dial
//
// ── Usage ─────────────────────────────────────────────────────────────────────
//   int   roomTemp = 25;
//   float voltage  = 3.31f;
//
//   DisplayGaugeInt   gTemp(&roomTemp, 0, 50,  "C");
//   DisplayGaugeInt   gTemp(&roomTemp, 0, 50,  "C", 270);         // showMinMax=true (default)
//   DisplayGaugeInt   gTemp(&roomTemp, 0, 50,  "C", 270, false);  // hide min/max labels
//
//   DisplayGaugeFloat gVolt(&voltage,  0.0f, 5.0f, "V");                    // defaults
//   DisplayGaugeFloat gVolt(&voltage,  0.0f, 5.0f, "V", 270, 2, false); // hide min/max
//   DisplayGaugeFloat gVolt(&voltage,  0.0f, 5.0f, "V", 270, 2);
//
//   menu.addDisplayGaugeInt  (menu1, "Room Temp", &gTemp);
//   menu.addDisplayGaugeFloat(menu1, "Voltage",   &gVolt);
// =============================================================================
#ifndef DISPLAY_GAUGE_H
#define DISPLAY_GAUGE_H

#include <Arduino.h>
#include "Adjusters.h"

// =============================================================================
// DisplayGaugeBase — shared logic; not used directly
// =============================================================================
class DisplayGaugeBase : public ValueAdjuster {
public:
    const char* unit;
    int         sweepDeg;
    bool        showMinMax;   // true = draw min/max labels beside arc endpoints

    DisplayGaugeBase(const char* u, int sweep, bool minmax)
        : unit(u),
          sweepDeg(sweep < 90 ? 90 : (sweep > 355 ? 355 : sweep)),
          showMinMax(minmax) {}

    // ── ValueAdjuster stubs (gauges are read-only) ────────────────────────────
    AdjusterType getType()    const override { return TYPE_GAUGE; }
    void         setValue(float)   override {}
    float        getIncrement()    override { return 1.0f; }
    const char*  getUnit()         override { return unit; }
    int          getDecimalPlaces()override { return 0; }

    // ── Arc geometry helpers (used by WidgetRenderer) ─────────────────────────
    // Gap centred at 270° (6 o'clock); arc sweeps clockwise from startAngle.
    float startAngle() const { return 270.0f - sweepDeg / 2.0f; }
    float endAngle()   const { return 270.0f + sweepDeg / 2.0f; }

    // ── Pure-virtual interface implemented by each subclass ───────────────────
    virtual float fraction()   const = 0;
    virtual float valueAngle() const = 0;

    // Format helpers called by WidgetRenderer (value, min, max labels)
    virtual void formatValue(char* buf, uint8_t sz) const = 0;
    virtual void formatMin  (char* buf, uint8_t sz) const = 0;
    virtual void formatMax  (char* buf, uint8_t sz) const = 0;

    // Menu-list right-side summary  e.g. "25C" or "3.3V"
    virtual void formatSummary(char* buf, uint8_t sz) const = 0;
};

// =============================================================================
// DisplayGaugeInt
// =============================================================================
class DisplayGaugeInt : public DisplayGaugeBase {
public:
    const int* valuePtr;
    int        minVal;
    int        maxVal;

    // DisplayGaugeInt(ptr, min, max, unit, sweepDeg, showMinMax)
    DisplayGaugeInt(const int*  ptr,
                    int         minV,
                    int         maxV,
                    const char* u        = "",
                    int         sweep    = 270,
                    bool        showMM   = true)
        : DisplayGaugeBase(u, sweep, showMM),
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
    float valueAngle() const override {
        return startAngle() + (float)sweepDeg * fraction();
    }

    void formatValue  (char* buf, uint8_t sz) const override { snprintf(buf, sz, "%d",  currentValue()); }
    void formatMin    (char* buf, uint8_t sz) const override { snprintf(buf, sz, "%d",  minVal); }
    void formatMax    (char* buf, uint8_t sz) const override { snprintf(buf, sz, "%d",  maxVal); }
    void formatSummary(char* buf, uint8_t sz) const override { snprintf(buf, sz, "%d%s",currentValue(), unit); }
};

// =============================================================================
// DisplayGaugeFloat
// =============================================================================
class DisplayGaugeFloat : public DisplayGaugeBase {
public:
    const float* valuePtr;
    float        minVal;
    float        maxVal;
    int          decimals;   // 0 = no decimal point (looks like int), 1, 2, …

    // DisplayGaugeFloat(ptr, min, max, unit, sweepDeg, decimals, showMinMax)
    DisplayGaugeFloat(const float* ptr,
                      float        minV,
                      float        maxV,
                      const char*  u        = "",
                      int          sweep    = 270,
                      int          dec      = 1,
                      bool         showMM   = true)
        : DisplayGaugeBase(u, sweep, showMM),
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
    float valueAngle() const override {
        return startAngle() + (float)sweepDeg * fraction();
    }

    void formatValue(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d",   (int)currentValue());
        else               _fmt(buf, sz, currentValue());
    }
    void formatMin(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d",   (int)minVal);
        else               _fmt(buf, sz, minVal);
    }
    void formatMax(char* buf, uint8_t sz) const override {
        if (decimals == 0) snprintf(buf, sz, "%d",   (int)maxVal);
        else               _fmt(buf, sz, maxVal);
    }
    void formatSummary(char* buf, uint8_t sz) const override {
        char tmp[12]; formatValue(tmp, sizeof(tmp));
        snprintf(buf, sz, "%s%s", tmp, unit);
    }

private:
    void _fmt(char* buf, uint8_t sz, float v) const {
        char fmt[10];
        snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
        snprintf(buf, sz, fmt, v);
    }
};

#endif // DISPLAY_GAUGE_H
