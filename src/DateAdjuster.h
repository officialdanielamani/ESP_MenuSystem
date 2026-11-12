// DateAdjuster.h
// Wheel-style date adjuster supporting any combination of day / month / year
// fields, user-defined field order, and numeric or named month display.
//
// ── Field flags (combine with | ) ────────────────────────────────────────────
//   DATE_DAY        show day   column  (01–31, clamped to month)
//   DATE_MONTH      show month column  (01–12 or Jan–Dec)
//   DATE_YEAR       show year  column  (userMin–userMax)
//
// ── Month format ─────────────────────────────────────────────────────────────
//   DATE_MONTH_NUM   → "08"
//   DATE_MONTH_SHORT → "Aug"
//   DATE_MONTH_LONG  → "August"
//
// ── Field order ──────────────────────────────────────────────────────────────
//   DATE_ORDER_DMY  → Day  Month Year   (default)
//   DATE_ORDER_MDY  → Month Day  Year
//   DATE_ORDER_YMD  → Year  Month Day
//
// ── Usage example ────────────────────────────────────────────────────────────
//   int d=21, m=8, y=2021;
//   DateAdjuster da(&d, &m, &y,
//                   DATE_DAY | DATE_MONTH | DATE_YEAR,
//                   DATE_ORDER_DMY, DATE_MONTH_NUM,
//                   2000, 2099);
//   menu.addDateMenuItem(settingsMenu, "Date", &da);
// =============================================================================
#ifndef DATE_ADJUSTER_H
#define DATE_ADJUSTER_H

#include <Arduino.h>
#include "Adjusters.h"

// ── Field flags ───────────────────────────────────────────────────────────────
#define DATE_DAY   0x01
#define DATE_MONTH 0x02
#define DATE_YEAR  0x04

// ── Month display format ──────────────────────────────────────────────────────
enum DateMonthFormat {
    DATE_MONTH_NUM   = 0,   // 08
    DATE_MONTH_SHORT = 1,   // Aug
    DATE_MONTH_LONG  = 2    // August
};

// ── Field render order ────────────────────────────────────────────────────────
enum DateOrder {
    DATE_ORDER_DMY = 0,   // 21 08 2021
    DATE_ORDER_MDY = 1,   // 08 21 2021
    DATE_ORDER_YMD = 2    // 2021 08 21
};

// ── Internal field ID ─────────────────────────────────────────────────────────
enum DateField { DF_DAY = 0, DF_MONTH = 1, DF_YEAR = 2 };

class DateAdjuster : public ValueAdjuster {
public:
    // ── Public state (read by WidgetRenderer) ─────────────────────────────────
    int*            dayPtr;
    int*            monthPtr;
    int*            yearPtr;
    uint8_t         fieldMask;      // DATE_DAY | DATE_MONTH | DATE_YEAR
    DateOrder       order;
    DateMonthFormat monthFormat;
    int             yearMin;
    int             yearMax;

    // Temp working copies (edited; written to ptrs only on confirm)
    int  tempDay;
    int  tempMonth;
    int  tempYear;

    // Snapshot taken at beginEdit() — Redo rewinds to this, NOT to stored ptr values
    int  snapDay;
    int  snapMonth;
    int  snapYear;

    // Which column (0..numCols-1) is active in the wheel editor
    uint8_t activeCol;

    // Resolved column sequence: fieldSequence[0..numCols-1] = DF_DAY/MONTH/YEAR
    uint8_t fieldSequence[3];
    uint8_t numCols;

    // ── Constructor ───────────────────────────────────────────────────────────
    DateAdjuster(int* day, int* month, int* year,
                 uint8_t fields      = DATE_DAY | DATE_MONTH | DATE_YEAR,
                 DateOrder ord       = DATE_ORDER_DMY,
                 DateMonthFormat mf  = DATE_MONTH_NUM,
                 int yMin = 2000, int yMax = 2099)
        : dayPtr(day), monthPtr(month), yearPtr(year),
          fieldMask(fields), order(ord), monthFormat(mf),
          yearMin(yMin), yearMax(yMax),
          tempDay(day ? *day : 1),
          tempMonth(month ? *month : 1),
          tempYear(year ? *year : 2000),
          snapDay(day ? *day : 1),
          snapMonth(month ? *month : 1),
          snapYear(year ? *year : 2000),
          activeCol(0), numCols(0)
    {
        buildSequence();
    }

    // ── ValueAdjuster interface ───────────────────────────────────────────────
    AdjusterType getType()         const override { return TYPE_DATE; }
    float        getValue()              override { return 0; }
    void         setValue(float)         override {}
    float        getIncrement()          override { return 1; }
    float        getMin()                override { return 0; }
    float        getMax()                override { return 0; }
    const char*  getUnit()               override { return ""; }
    int          getDecimalPlaces()      override { return 0; }

    // ── Editor lifecycle ──────────────────────────────────────────────────────
    void beginEdit() {
        tempDay   = dayPtr   ? *dayPtr   : 1;
        tempMonth = monthPtr ? *monthPtr : 1;
        tempYear  = yearPtr  ? *yearPtr  : yearMin;
        // Snapshot the opening values — Redo returns here, not to stored ptr
        snapDay   = tempDay;
        snapMonth = tempMonth;
        snapYear  = tempYear;
        activeCol = 0;
    }

    void confirm() {
        if (dayPtr   && (fieldMask & DATE_DAY))   *dayPtr   = tempDay;
        if (monthPtr && (fieldMask & DATE_MONTH))  *monthPtr = tempMonth;
        if (yearPtr  && (fieldMask & DATE_YEAR))   *yearPtr  = tempYear;
    }

    // activeCol values:
    //   0 .. numCols-1  → field columns  (date row)
    //   numCols         → "Redo"  row    (reset temp back to snapshot)
    //   numCols+1       → "Edit"  row    (go back to fields keeping changes)
    //   numCols+2       → "Save"  row    (confirm, write to pointers)
    //   numCols+3       → "Back"  row    (cancel, discard)
    bool isOnField() const { return activeCol < numCols; }
    bool isOnRedo()  const { return activeCol == numCols; }
    bool isOnEdit()  const { return activeCol == numCols + 1; }
    bool isOnSave()  const { return activeCol == numCols + 2; }
    bool isOnBack()  const { return activeCol == numCols + 3; }

    // For backward compatibility
    bool isOnSet()   const { return isOnSave(); }

    // Advance through field columns (called by OK on a field)
    void nextCol() { if (activeCol < numCols + 3) activeCol++; }

    // Move within action rows only (called by Up/Down when NOT on a field).
    // Wraps Redo <-> Edit <-> Save <-> Back without spilling back into field columns.
    void actionUp() {
        if (activeCol > numCols) activeCol--;
        else activeCol = numCols + 3;  // wrap: Redo -> Back
    }
    void actionDown() {
        if (activeCol < numCols + 3) activeCol++;
        else activeCol = numCols;      // wrap: Back -> Redo
    }

    // Redo: reset temp back to the values when the editor was opened.
    void redo() {
        tempDay   = snapDay;
        tempMonth = snapMonth;
        tempYear  = snapYear;
        activeCol = 0;   // restart from first field
    }

    // Edit: keep current temp values and go back to field editing
    void edit() {
        activeCol = 0;
    }

    // ── Scroll active column up (increment) ───────────────────────────────────
    void scrollUp() {
        DateField f = currentField();
        switch (f) {
            case DF_DAY:
                tempDay++;
                if (tempDay > daysInMonth(tempMonth, tempYear)) tempDay = 1;
                break;
            case DF_MONTH:
                tempMonth++;
                if (tempMonth > 12) tempMonth = 1;
                clampDay();
                break;
            case DF_YEAR:
                if (tempYear < yearMax) { tempYear++; clampDay(); }
                break;
        }
    }

    // ── Scroll active column down (decrement) ─────────────────────────────────
    void scrollDown() {
        DateField f = currentField();
        switch (f) {
            case DF_DAY:
                tempDay--;
                if (tempDay < 1) tempDay = daysInMonth(tempMonth, tempYear);
                break;
            case DF_MONTH:
                tempMonth--;
                if (tempMonth < 1) tempMonth = 12;
                clampDay();
                break;
            case DF_YEAR:
                if (tempYear > yearMin) { tempYear--; clampDay(); }
                break;
        }
    }

    // ── Formatting helpers ────────────────────────────────────────────────────

    // Format a single field value into buf.
    void formatField(DateField f, char* buf, uint8_t sz) const {
        switch (f) {
            case DF_DAY:
                snprintf(buf, sz, "%02d", tempDay);
                break;
            case DF_MONTH:
                formatMonth(tempMonth, buf, sz);
                break;
            case DF_YEAR:
                snprintf(buf, sz, "%04d", tempYear);
                break;
        }
    }

    // Format a compact summary string for the menu list (uses stored values).
    void formatSummary(char* buf, uint8_t sz) const {
        char tmp[8];
        buf[0] = '\0';
        for (uint8_t c = 0; c < numCols; c++) {
            if (c > 0 && strlen(buf) + 1 < sz) strcat(buf, " ");
            DateField f = (DateField)fieldSequence[c];
            int val = fieldVal(f);
            switch (f) {
                case DF_DAY:   snprintf(tmp, sizeof(tmp), "%02d", val); break;
                case DF_MONTH: formatMonthStored(val, tmp, sizeof(tmp)); break;
                case DF_YEAR:  snprintf(tmp, sizeof(tmp), "%04d", val); break;
            }
            strncat(buf, tmp, sz - strlen(buf) - 1);
        }
    }

    // ── Accessors ─────────────────────────────────────────────────────────────
    DateField currentField() const {
        return (activeCol < numCols) ? (DateField)fieldSequence[activeCol] : DF_DAY;
    }

    // Width in characters of a given field (for column layout)
    uint8_t fieldWidth(DateField f) const {
        switch (f) {
            case DF_DAY:   return 2;
            case DF_MONTH:
                if (monthFormat == DATE_MONTH_NUM)   return 2;
                if (monthFormat == DATE_MONTH_SHORT) return 3;
                return 9;  // longest = "September"
            case DF_YEAR:  return 4;
        }
        return 2;
    }

private:
    // Build fieldSequence[] from order + mask
    void buildSequence() {
        numCols = 0;
        // Define the order-dependent slot layout
        static const DateField orderTable[3][3] = {
            { DF_DAY,   DF_MONTH, DF_YEAR },  // DATE_ORDER_DMY
            { DF_MONTH, DF_DAY,   DF_YEAR },  // DATE_ORDER_MDY
            { DF_YEAR,  DF_MONTH, DF_DAY  }   // DATE_ORDER_YMD
        };
        int o = (int)order;
        if (o < 0 || o > 2) o = 0;
        for (int i = 0; i < 3; i++) {
            DateField f = orderTable[o][i];
            uint8_t   flag = (f == DF_DAY)   ? DATE_DAY   :
                             (f == DF_MONTH)  ? DATE_MONTH : DATE_YEAR;
            if (fieldMask & flag)
                fieldSequence[numCols++] = (uint8_t)f;
        }
        if (numCols == 0) {
            // Fallback: show all
            fieldMask = DATE_DAY | DATE_MONTH | DATE_YEAR;
            buildSequence();
        }
    }

    int fieldVal(DateField f) const {
        switch (f) {
            case DF_DAY:   return dayPtr   ? *dayPtr   : tempDay;
            case DF_MONTH: return monthPtr ? *monthPtr : tempMonth;
            case DF_YEAR:  return yearPtr  ? *yearPtr  : tempYear;
        }
        return 0;
    }

    void clampDay() {
        int maxD = daysInMonth(tempMonth, tempYear);
        if (tempDay > maxD) tempDay = maxD;
    }

    static int daysInMonth(int m, int y) {
        if (m == 2) return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
        static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        return (m >= 1 && m <= 12) ? days[m] : 30;
    }

    void formatMonth(int m, char* buf, uint8_t sz) const {
        static const char* shortNames[] = {
            "","Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"
        };
        static const char* longNames[] = {
            "","January","February","March","April","May","June",
            "July","August","September","October","November","December"
        };
        if (m < 1 || m > 12) { snprintf(buf, sz, "??"); return; }
        switch (monthFormat) {
            case DATE_MONTH_NUM:   snprintf(buf, sz, "%02d", m); break;
            case DATE_MONTH_SHORT: snprintf(buf, sz, "%s", shortNames[m]); break;
            case DATE_MONTH_LONG:  snprintf(buf, sz, "%s", longNames[m]);  break;
        }
    }

    void formatMonthStored(int m, char* buf, uint8_t sz) const {
        // Same as formatMonth but uses stored ptr value
        formatMonth(m, buf, sz);
    }
};

#endif // DATE_ADJUSTER_H
