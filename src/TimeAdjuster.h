// TimeAdjuster.h
// Wheel-style time adjuster supporting any combination of hour / minute / second
// fields, 12/24-hour clock mode, am/pm selection, and per-field range + step.
//
// ── Field flags (combine with | ) ────────────────────────────────────────────
//   TIME_HOUR   show hour   column  (00–23 in 24 h, 01–12 in 12 h)
//   TIME_MINUTE show minute column  (00–59 or user range)
//   TIME_SECOND show second column  (00–59 or user range)
//
// ── Clock mode ────────────────────────────────────────────────────────────────
//   TIME_MODE_24   24-hour clock, no am/pm column         (default)
//   TIME_MODE_12   12-hour clock, am/pm column appended automatically
//
// ── Field layout examples ────────────────────────────────────────────────────
//   TIME_HOUR | TIME_MINUTE | TIME_SECOND  (24 h default) → 13 | 32 | 43
//   TIME_HOUR | TIME_MINUTE               (24 h)          → 13 | 32
//   TIME_HOUR | TIME_MINUTE               (12 h)          → 01 | 32 | am
//   TIME_SECOND                                           → 44
//
// ── Per-field range & step ────────────────────────────────────────────────────
//   Default: full natural range, step 1.
//   Use setHourRange / setMinuteRange / setSecondRange to restrict.
//   Hours must always be supplied in 24-hour values (0–23) even in 12-h mode;
//   the adjuster handles the conversion internally.
//
//   Example:
//     ta.setHourRange(4, 12, 1);     // 04..12, step 1  (24 h values)
//     ta.setMinuteRange(5, 30, 5);   // 05..30, step 5
//     ta.setSecondRange(0, 60, 2);   // 00..60, step 2
//
// ── Usage example ────────────────────────────────────────────────────────────
//   int h=13, m=32, s=43;
//   TimeAdjuster ta(&h, &m, &s,
//                   TIME_HOUR | TIME_MINUTE | TIME_SECOND,
//                   TIME_MODE_24);
//   menu.addTimeMenuItem(settingsMenu, "Time", &ta);
//
//   // 12-hour with custom minute step
//   int h=1, m=0, s=0;
//   TimeAdjuster ta12(&h, &m, nullptr,
//                     TIME_HOUR | TIME_MINUTE,
//                     TIME_MODE_12);
//   ta12.setMinuteRange(0, 59, 5);
//   menu.addTimeMenuItem(settingsMenu, "Alarm", &ta12);
// =============================================================================
#ifndef TIME_ADJUSTER_H
#define TIME_ADJUSTER_H

#include <Arduino.h>
#include "Adjusters.h"

// ── Field flags ───────────────────────────────────────────────────────────────
#define TIME_HOUR   0x01
#define TIME_MINUTE 0x02
#define TIME_SECOND 0x04

// ── Clock mode ────────────────────────────────────────────────────────────────
enum TimeClockMode {
    TIME_MODE_24 = 0,   // 24-hour (no am/pm)
    TIME_MODE_12 = 1    // 12-hour (am/pm column appended)
};

// ── Internal field IDs ────────────────────────────────────────────────────────
// TF_AMPM is synthesised automatically when mode == TIME_MODE_12
enum TimeField { TF_HOUR = 0, TF_MINUTE = 1, TF_SECOND = 2, TF_AMPM = 3 };

// =============================================================================
class TimeAdjuster : public ValueAdjuster {
public:
    // ── Public state (read by WidgetRenderer / MenuSystem) ────────────────────
    int*          hourPtr;
    int*          minutePtr;
    int*          secondPtr;
    uint8_t       fieldMask;        // TIME_HOUR | TIME_MINUTE | TIME_SECOND
    TimeClockMode clockMode;

    // Working copies (edited in-place; written to ptrs only on confirm)
    int  tempHour;      // always stored in 24-h (0–23)
    int  tempMinute;
    int  tempSecond;
    bool tempPm;        // true = PM  (only meaningful in 12-h mode)

    // Snapshot taken at beginEdit() so Redo can return to opening state
    int  snapHour;
    int  snapMinute;
    int  snapSecond;
    bool snapPm;

    // Active column index
    // 0 .. numCols-1  → field columns (time row)
    // numCols         → Redo
    // numCols+1       → Set
    // numCols+2       → Back
    uint8_t activeCol;

    // Resolved column sequence: fieldSequence[0..numCols-1] = TF_HOUR/MIN/SEC/AMPM
    uint8_t fieldSequence[4];
    uint8_t numCols;

    // ── Per-field range & step ────────────────────────────────────────────────
    struct FieldRange {
        int     minVal;
        int     maxVal;
        int     step;
    };
    FieldRange hourRange;       // 24-h values always
    FieldRange minuteRange;
    FieldRange secondRange;

    // ── Constructor ───────────────────────────────────────────────────────────
    // secondPtr may be nullptr to omit seconds even if TIME_SECOND is absent
    // from fields (or just omit TIME_SECOND from the field mask).
    TimeAdjuster(int*          hour,
                 int*          minute,
                 int*          second   = nullptr,
                 uint8_t       fields   = TIME_HOUR | TIME_MINUTE | TIME_SECOND,
                 TimeClockMode mode     = TIME_MODE_24)
        : hourPtr(hour), minutePtr(minute), secondPtr(second),
          fieldMask(fields), clockMode(mode),
          tempHour  (hour   ? *hour   : 0),
          tempMinute(minute ? *minute : 0),
          tempSecond(second ? *second : 0),
          tempPm(false),
          snapHour(0), snapMinute(0), snapSecond(0), snapPm(false),
          activeCol(0), numCols(0)
    {
        // Default full-range steps
        hourRange   = { 0,  23, 1 };
        minuteRange = { 0,  59, 1 };
        secondRange = { 0,  59, 1 };

        // Derive initial am/pm from stored hour
        if (hour) {
            int h = *hour;
            if (h < 0) h = 0;
            if (h > 23) h = 23;
            tempHour = h;
            tempPm   = (h >= 12);
        }

        buildSequence();
    }

    // ── Per-field range configuration ─────────────────────────────────────────
    // Hours are always in 24-h values (0–23).
    void setHourRange  (int minH, int maxH, int step = 1) {
        hourRange = { constrain(minH, 0, 23), constrain(maxH, 0, 23), max(1, step) };
    }
    void setMinuteRange(int minM, int maxM, int step = 1) {
        minuteRange = { constrain(minM, 0, 59), constrain(maxM, 0, 59), max(1, step) };
    }
    void setSecondRange(int minS, int maxS, int step = 1) {
        secondRange = { constrain(minS, 0, 59), constrain(maxS, 0, 59), max(1, step) };
    }

    // ── ValueAdjuster interface stubs ─────────────────────────────────────────
    AdjusterType getType()          const override { return TYPE_TIME; }
    float        getValue()               override { return 0; }
    void         setValue(float)          override {}
    float        getIncrement()           override { return 1; }
    float        getMin()                 override { return 0; }
    float        getMax()                 override { return 0; }
    const char*  getUnit()                override { return ""; }
    int          getDecimalPlaces()       override { return 0; }

    // ── Editor lifecycle ──────────────────────────────────────────────────────
    void beginEdit() {
        tempHour   = hourPtr   ? *hourPtr   : hourRange.minVal;
        tempMinute = minutePtr ? *minutePtr : minuteRange.minVal;
        tempSecond = secondPtr ? *secondPtr : secondRange.minVal;

        // Clamp loaded values to configured ranges (snap to nearest step)
        tempHour   = clampToRange(tempHour,   hourRange);
        tempMinute = clampToRange(tempMinute, minuteRange);
        tempSecond = clampToRange(tempSecond, secondRange);

        tempPm = (clockMode == TIME_MODE_12) && (tempHour >= 12);

        // Snapshot
        snapHour   = tempHour;
        snapMinute = tempMinute;
        snapSecond = tempSecond;
        snapPm     = tempPm;

        activeCol = 0;
    }

    void confirm() {
        if (hourPtr   && (fieldMask & TIME_HOUR))   *hourPtr   = tempHour;
        if (minutePtr && (fieldMask & TIME_MINUTE))  *minutePtr = tempMinute;
        if (secondPtr && (fieldMask & TIME_SECOND))  *secondPtr = tempSecond;
    }

    // ── Column / row navigation helpers ──────────────────────────────────────
    // Action rows: numCols=Redo, +1=Edit, +2=Save, +3=Back
    bool isOnField() const { return activeCol < numCols; }
    bool isOnRedo()  const { return activeCol == numCols; }
    bool isOnEdit()  const { return activeCol == numCols + 1; }
    bool isOnSave()  const { return activeCol == numCols + 2; }
    bool isOnBack()  const { return activeCol == numCols + 3; }

    // For backward compatibility
    bool isOnSet()   const { return isOnSave(); }

    void nextCol() { if (activeCol < numCols + 3) activeCol++; }

    void actionUp() {
        if (activeCol > numCols) activeCol--;
        else activeCol = numCols + 3;   // wrap: Redo → Back
    }
    void actionDown() {
        if (activeCol < numCols + 3) activeCol++;
        else activeCol = numCols;       // wrap: Back → Redo
    }

    void redo() {
        tempHour   = snapHour;
        tempMinute = snapMinute;
        tempSecond = snapSecond;
        tempPm     = snapPm;
        activeCol  = 0;
    }

    // Edit: keep current temp values and go back to field editing
    void edit() {
        activeCol = 0;
    }

    // ── Scroll active column up (increment) ──────────────────────────────────
    void scrollUp() {
        TimeField f = currentField();
        switch (f) {
            case TF_HOUR:
                if (clockMode == TIME_MODE_12) {
                    // Scroll in 12-h space (1–12), keep pm flag
                    int h12 = to12h(tempHour);
                    h12 += hourRange.step;
                    // Determine 12-h range boundaries
                    int lo = to12h(hourRange.minVal);
                    int hi = to12h(hourRange.maxVal);
                    if (lo == 0) lo = 12;
                    if (hi == 0) hi = 12;
                    if (h12 > hi) h12 = lo;
                    tempHour = to24h(h12, tempPm);
                } else {
                    tempHour += hourRange.step;
                    if (tempHour > hourRange.maxVal) tempHour = hourRange.minVal;
                }
                break;
            case TF_MINUTE:
                tempMinute += minuteRange.step;
                if (tempMinute > minuteRange.maxVal) tempMinute = minuteRange.minVal;
                break;
            case TF_SECOND:
                tempSecond += secondRange.step;
                if (tempSecond > secondRange.maxVal) tempSecond = secondRange.minVal;
                break;
            case TF_AMPM:
                // Toggle pm and adjust tempHour accordingly
                tempPm = !tempPm;
                if (tempPm && tempHour < 12)  tempHour += 12;
                if (!tempPm && tempHour >= 12) tempHour -= 12;
                break;
        }
    }

    // ── Scroll active column down (decrement) ─────────────────────────────────
    void scrollDown() {
        TimeField f = currentField();
        switch (f) {
            case TF_HOUR:
                if (clockMode == TIME_MODE_12) {
                    int h12 = to12h(tempHour);
                    h12 -= hourRange.step;
                    int lo = to12h(hourRange.minVal);
                    int hi = to12h(hourRange.maxVal);
                    if (lo == 0) lo = 12;
                    if (hi == 0) hi = 12;
                    if (h12 < lo) h12 = hi;
                    tempHour = to24h(h12, tempPm);
                } else {
                    tempHour -= hourRange.step;
                    if (tempHour < hourRange.minVal) tempHour = hourRange.maxVal;
                }
                break;
            case TF_MINUTE:
                tempMinute -= minuteRange.step;
                if (tempMinute < minuteRange.minVal) tempMinute = minuteRange.maxVal;
                break;
            case TF_SECOND:
                tempSecond -= secondRange.step;
                if (tempSecond < secondRange.minVal) tempSecond = secondRange.maxVal;
                break;
            case TF_AMPM:
                tempPm = !tempPm;
                if (tempPm && tempHour < 12)  tempHour += 12;
                if (!tempPm && tempHour >= 12) tempHour -= 12;
                break;
        }
    }

    // ── Formatting helpers ────────────────────────────────────────────────────

    // Format a single field value into buf (uses current temp values).
    void formatField(TimeField f, char* buf, uint8_t sz) const {
        switch (f) {
            case TF_HOUR:
                if (clockMode == TIME_MODE_12) {
                    int h = to12h(tempHour);
                    snprintf(buf, sz, "%02d", h);
                } else {
                    snprintf(buf, sz, "%02d", tempHour);
                }
                break;
            case TF_MINUTE:
                snprintf(buf, sz, "%02d", tempMinute);
                break;
            case TF_SECOND:
                snprintf(buf, sz, "%02d", tempSecond);
                break;
            case TF_AMPM:
                snprintf(buf, sz, "%s", tempPm ? "pm" : "am");
                break;
        }
    }

    // Format a compact summary string for the menu list (uses stored ptr values).
    // Examples:  "13:32:43"   "01:32 pm"   "44"
    void formatSummary(char* buf, uint8_t sz) const {
        buf[0] = '\0';
        bool first = true;
        for (uint8_t c = 0; c < numCols; c++) {
            TimeField f = (TimeField)fieldSequence[c];

            if (!first && strlen(buf) + 1 < sz) {
                if (f == TF_AMPM)
                    strncat(buf, " ", sz - strlen(buf) - 1);
                else
                    strncat(buf, ":", sz - strlen(buf) - 1);
            }
            first = false;

            char tmp[8];
            switch (f) {
                case TF_HOUR: {
                    int hv = hourPtr ? *hourPtr : tempHour;
                    if (clockMode == TIME_MODE_12) snprintf(tmp, sizeof(tmp), "%02d", to12h(hv));
                    else                            snprintf(tmp, sizeof(tmp), "%02d", hv);
                    break;
                }
                case TF_MINUTE:
                    snprintf(tmp, sizeof(tmp), "%02d", minutePtr ? *minutePtr : tempMinute);
                    break;
                case TF_SECOND:
                    snprintf(tmp, sizeof(tmp), "%02d", secondPtr ? *secondPtr : tempSecond);
                    break;
                case TF_AMPM: {
                    int hv = hourPtr ? *hourPtr : tempHour;
                    snprintf(tmp, sizeof(tmp), "%s", (hv >= 12) ? "pm" : "am");
                    break;
                }
            }
            strncat(buf, tmp, sz - strlen(buf) - 1);
        }
    }

    // ── Accessors ─────────────────────────────────────────────────────────────
    TimeField currentField() const {
        return (activeCol < numCols)
               ? (TimeField)fieldSequence[activeCol]
               : TF_HOUR;
    }

    // Character width of a given field column (for layout calculations)
    uint8_t fieldWidth(TimeField f) const {
        switch (f) {
            case TF_HOUR:   return 2;
            case TF_MINUTE: return 2;
            case TF_SECOND: return 2;
            case TF_AMPM:   return 2;   // "am" / "pm"
        }
        return 2;
    }

private:
    // ── Build column sequence from fieldMask ──────────────────────────────────
    // Order is always: Hour → Minute → Second → AM/PM (if 12-h mode)
    void buildSequence() {
        numCols = 0;
        if (fieldMask & TIME_HOUR)   fieldSequence[numCols++] = TF_HOUR;
        if (fieldMask & TIME_MINUTE) fieldSequence[numCols++] = TF_MINUTE;
        if (fieldMask & TIME_SECOND) fieldSequence[numCols++] = TF_SECOND;
        // AM/PM column is appended automatically in 12-h mode when hours are shown
        if (clockMode == TIME_MODE_12 && (fieldMask & TIME_HOUR))
            fieldSequence[numCols++] = TF_AMPM;
        if (numCols == 0) {
            // Fallback: show everything
            fieldMask = TIME_HOUR | TIME_MINUTE | TIME_SECOND;
            buildSequence();
        }
    }

    // ── 12/24-h conversion helpers ────────────────────────────────────────────
    // Convert 24-h value (0–23) to 12-h display value (1–12)
    static int to12h(int h24) {
        int h = h24 % 12;
        return (h == 0) ? 12 : h;
    }
    // Convert 12-h display value (1–12) + pm flag back to 24-h (0–23)
    static int to24h(int h12, bool pm) {
        if (h12 == 12) h12 = 0;
        return pm ? h12 + 12 : h12;
    }

    // Clamp value to range; also snaps to nearest valid step position
    static int clampToRange(int val, const FieldRange& r) {
        if (val < r.minVal) return r.minVal;
        if (val > r.maxVal) return r.maxVal;
        // Snap to grid of steps starting from minVal
        int offset = val - r.minVal;
        offset = (offset / r.step) * r.step;
        return r.minVal + offset;
    }
};

#endif // TIME_ADJUSTER_H
