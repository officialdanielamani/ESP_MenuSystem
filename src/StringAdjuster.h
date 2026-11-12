// StringAdjuster.h
// Character-by-character string input adjuster for the ESP32 MenuSystem.
//
// ── Design ───────────────────────────────────────────────────────────────────
//   Edits one character position at a time. Up/Down scrolls through the
//   allowed character set. OK advances to the next character position or
//   moves to the action rows (Redo / Done / Back).
//
//   The character set is fully user-defined via a const char* palette.
//   Five control entries appear BEFORE the user characters in scroll order:
//     "OK"    – accept current char, advance to next position
//     "BACK"  – go back to previous character position
//     "DEL"   – delete character at current position
//     "SPACE" – insert a space character
//     "DONE"  – done inputting (only visible when tempLen >= minLength)
//
//   After selecting DONE, four action rows appear:
//     Redo  – reset temp buffer back to the snapshot taken at beginEdit()
//     Edit  – go back to character input keeping current changes
//     Save  – confirm: copy temp buffer into the target char* pointer
//     Back  – cancel: discard temp buffer, exit editor
//
// ── Memory ───────────────────────────────────────────────────────────────────
//   Uses a char[MAX_STRING_LENGTH+1] temp buffer internally. No heap.
//   The palette (allowed chars) is stored as a const char* pointer — the
//   caller owns the string and it must remain valid.
//
// ── Usage ────────────────────────────────────────────────────────────────────
//   char myName[33] = "Hello";
//
//   // Letters + digits, 1–16 characters
//   StringAdjuster sa(myName, STRING_PALETTE_ALPHANUM, 1, 16);
//
//   // Upper + digits + symbols (combine by placing next to each other)
//   StringAdjuster sa(buf, STRING_PALETTE_UPPER STRING_PALETTE_DIGITS STRING_PALETTE_SYMBOL, 0, 20);
//
//   // Digits only, 4 chars exactly (e.g. PIN code)
//   StringAdjuster pinAdj(pin, STRING_PALETTE_DIGITS, 4, 4);
//
//   // Phone number with custom prefix chars
//   StringAdjuster phoneAdj(buf, "+-()" STRING_PALETTE_DIGITS, 1, 20);
//
//   // Hex input
//   StringAdjuster hexAdj(buf, STRING_PALETTE_HEX, 0, 8);
//
//   menu.addStringMenuItem(settingsMenu, "Name", &sa);
//
// ── Predefined palettes (combine by placing next to each other) ──────────────
//   Building blocks:
//     STRING_PALETTE_UPPER      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
//     STRING_PALETTE_LOWER      "abcdefghijklmnopqrstuvwxyz"
//     STRING_PALETTE_DIGITS     "0123456789"
//     STRING_PALETTE_HEX_AF     "ABCDEF"
//     STRING_PALETTE_SYMBOL     "!@#$%^&*()-_=+.,?/"
//   Ready-made combos:
//     STRING_PALETTE_ALPHA      = UPPER + LOWER
//     STRING_PALETTE_ALPHANUM   = UPPER + LOWER + DIGITS
//     STRING_PALETTE_HEX        = DIGITS + HEX_AF
// =============================================================================
#ifndef STRING_ADJUSTER_H
#define STRING_ADJUSTER_H

#include <Arduino.h>
#include "Adjusters.h"

#define MAX_STRING_LENGTH 32

// ── Predefined palettes ──────────────────────────────────────────────────────
// These are raw string literals designed to be COMBINED by placing them
// next to each other. In C/C++, adjacent string literals are automatically
// concatenated by the compiler:
//
//   "ABC" "DEF"  →  "ABCDEF"   (no + operator needed)
//
// ── Building blocks ──────────────────────────────────────────────────────────
#define STRING_PALETTE_UPPER    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define STRING_PALETTE_LOWER    "abcdefghijklmnopqrstuvwxyz"
#define STRING_PALETTE_DIGITS   "0123456789"
#define STRING_PALETTE_HEX_AF   "ABCDEF"
#define STRING_PALETTE_SYMBOL   "!@#$%^&*()-_=+.,?/"

// ── Ready-made combos (for convenience) ──────────────────────────────────────
#define STRING_PALETTE_ALPHA    STRING_PALETTE_UPPER STRING_PALETTE_LOWER
#define STRING_PALETTE_ALPHANUM STRING_PALETTE_UPPER STRING_PALETTE_LOWER STRING_PALETTE_DIGITS
#define STRING_PALETTE_HEX      STRING_PALETTE_DIGITS STRING_PALETTE_HEX_AF
//
// ── Usage examples ───────────────────────────────────────────────────────────
//
//   // Upper + digits only:
//   StringAdjuster sa(buf, STRING_PALETTE_UPPER STRING_PALETTE_DIGITS, 1, 16);
//
//   // Everything:
//   StringAdjuster sa(buf, STRING_PALETTE_ALPHANUM STRING_PALETTE_SYMBOL, 0, 32);
//
//   // Phone number:
//   StringAdjuster sa(buf, "+-()" STRING_PALETTE_DIGITS, 1, 20);
//
//   // Custom:
//   StringAdjuster sa(buf, "ACGT", 1, 8);  // DNA bases
//

class StringAdjuster : public ValueAdjuster {
public:
    // ── Public state (read by WidgetRenderer / MenuSystem) ────────────────────
    char*       valuePtr;           // target string (caller-owned, MAX_STRING_LENGTH+1)
    const char* palette;            // allowed characters (caller-owned, must stay valid)
    uint8_t     paletteLen;         // cached strlen(palette)
    uint8_t     minLength;          // minimum string length (0 = empty allowed)
    uint8_t     maxLength;          // maximum string length (capped to MAX_STRING_LENGTH)

    // Working copy (edited in-place; written to valuePtr only on confirm)
    char    tempBuf[MAX_STRING_LENGTH + 1];
    uint8_t tempLen;                // current length of tempBuf (excluding null)

    // Snapshot taken at beginEdit() — Redo returns here
    char    snapBuf[MAX_STRING_LENGTH + 1];
    uint8_t snapLen;

    // ── Editor navigation state ──────────────────────────────────────────────
    // Phase 1: character input
    //   editPos    = which character position we're editing (0..tempLen)
    //   charIndex  = index into the extended palette for the current position
    //
    // Phase 2: action rows (after DN)
    //   actionRow  = 0 (Redo), 1 (Edit), 2 (Clear All), 3 (Save), 4 (Back)
    //
    // Phase 3: Clear All confirm dialog
    //   confirmRow = 0 (Yes), 1 (No)
    //
    // inActionPhase = false → phase 1, true → phase 2
    // inConfirmClear = true → phase 3 (Yes/No clear dialog)
    bool     inActionPhase;
    bool     inConfirmClear;  // true when showing "Clear All?" Yes/No dialog
    uint8_t  confirmRow;      // 0 = Yes, 1 = No
    uint8_t  editPos;
    uint8_t  charIndex;     // 0..paletteLen-1 = chars, paletteLen = [ ], paletteLen+1 = OK, paletteLen+2 = DN
    uint8_t  actionRow;     // 0 = Redo, 1 = Edit, 2 = Clear All, 3 = Save, 4 = Back

    // ── Constructor ───────────────────────────────────────────────────────────
    StringAdjuster(char* value, const char* allowedChars,
                   uint8_t minLen = 0, uint8_t maxLen = MAX_STRING_LENGTH)
        : valuePtr(value),
          palette(allowedChars),
          paletteLen(0),
          minLength(minLen),
          maxLength(maxLen > MAX_STRING_LENGTH ? MAX_STRING_LENGTH : maxLen),
          tempLen(0),
          snapLen(0),
          inActionPhase(false),
          inConfirmClear(false),
          confirmRow(1),
          editPos(0),
          charIndex(0),
          actionRow(0)
    {
        paletteLen = (uint8_t)strlen(allowedChars);
        if (paletteLen > 200) paletteLen = 200;  // sanity cap

        // Copy initial value into temp and snap
        if (value) {
            strncpy(tempBuf, value, MAX_STRING_LENGTH);
            tempBuf[MAX_STRING_LENGTH] = '\0';
            tempLen = (uint8_t)strlen(tempBuf);
        } else {
            tempBuf[0] = '\0';
            tempLen = 0;
        }
        memcpy(snapBuf, tempBuf, MAX_STRING_LENGTH + 1);
        snapLen = tempLen;
    }

    // ── ValueAdjuster interface stubs ─────────────────────────────────────────
    AdjusterType getType()         const override { return TYPE_STRING; }
    float        getValue()              override { return 0; }
    void         setValue(float)         override {}
    float        getIncrement()          override { return 1; }
    float        getMin()                override { return 0; }
    float        getMax()                override { return 0; }
    const char*  getUnit()               override { return ""; }
    int          getDecimalPlaces()      override { return 0; }

    // ── Editor lifecycle ──────────────────────────────────────────────────────
    void beginEdit() {
        // Load current value from pointer
        if (valuePtr) {
            strncpy(tempBuf, valuePtr, MAX_STRING_LENGTH);
            tempBuf[MAX_STRING_LENGTH] = '\0';
            tempLen = (uint8_t)strlen(tempBuf);
        } else {
            tempBuf[0] = '\0';
            tempLen = 0;
        }
        // Snapshot
        memcpy(snapBuf, tempBuf, MAX_STRING_LENGTH + 1);
        snapLen = tempLen;

        // Start at position 0
        editPos       = 0;
        inActionPhase = false;
        inConfirmClear = false;
        confirmRow    = 1;
        actionRow     = 0;
        syncCharIndex();
    }

    void confirm() {
        if (valuePtr) {
            strncpy(valuePtr, tempBuf, MAX_STRING_LENGTH);
            valuePtr[MAX_STRING_LENGTH] = '\0';
        }
    }

    void redo() {
        memcpy(tempBuf, snapBuf, MAX_STRING_LENGTH + 1);
        tempLen       = snapLen;
        editPos       = 0;
        inActionPhase = false;
        inConfirmClear = false;
        confirmRow    = 1;
        actionRow     = 0;
        syncCharIndex();
    }

    // Edit: keep current tempBuf (user's changes) and go back to char input
    void edit() {
        editPos        = 0;
        inActionPhase  = false;
        inConfirmClear = false;
        confirmRow     = 1;
        actionRow      = 0;
        syncCharIndex();
    }

    // ── Phase queries ─────────────────────────────────────────────────────────
    // Action rows: 0=Redo, 1=Edit, 2=Clear All, 3=Save, 4=Back
    bool isOnChar()     const { return !inActionPhase && !inConfirmClear; }
    bool isOnRedo()     const { return inActionPhase && !inConfirmClear && actionRow == 0; }
    bool isOnEdit()     const { return inActionPhase && !inConfirmClear && actionRow == 1; }
    bool isOnClearAll() const { return inActionPhase && !inConfirmClear && actionRow == 2; }
    bool isOnSave()     const { return inActionPhase && !inConfirmClear && actionRow == 3; }
    bool isOnBack()     const { return inActionPhase && !inConfirmClear && actionRow == 4; }
    bool isOnConfirm()  const { return inConfirmClear; }
    bool confirmIsYes() const { return inConfirmClear && confirmRow == 0; }
    bool confirmIsNo()  const { return inConfirmClear && confirmRow == 1; }

    // Enter clear-all confirm dialog
    void beginConfirmClear() {
        inConfirmClear = true;
        confirmRow = 1;   // default to No (safer)
    }

    // Execute clear all (called after user confirms Yes)
    void clearAll() {
        tempBuf[0] = '\0';
        tempLen    = 0;
        editPos    = 0;
        inActionPhase  = false;
        inConfirmClear = false;
        confirmRow     = 1;
        actionRow      = 0;
        syncCharIndex();
    }

    // Cancel clear-all dialog (No selected)
    void cancelConfirmClear() {
        inConfirmClear = false;
        confirmRow     = 1;
    }

    // Navigate confirm dialog
    void confirmUp()   { confirmRow = (confirmRow > 0) ? confirmRow - 1 : 1; }
    void confirmDown() { confirmRow = (confirmRow < 1) ? confirmRow + 1 : 0; }

    // ── Extended palette ──────────────────────────────────────────────────────
    // Index layout (controls appear FIRST, then user characters):
    //   0             = "OK"  (accept current char, advance to next position)
    //   1             = "BC"  (go back to previous character position)
    //   2             = "DL"  (delete character at current position)
    //   3             = "[ ]" (insert a space character)
    //   4             = "DN"  (done — only visible when tempLen >= minLength)
    //   CTRL_COUNT..  = user palette characters
    //
    // When DN is hidden (tempLen < minLength), it is skipped during scroll.

    static const uint8_t CTRL_OK  = 0;
    static const uint8_t CTRL_BC  = 1;
    static const uint8_t CTRL_DL  = 2;
    static const uint8_t CTRL_SPC = 3;
    static const uint8_t CTRL_DN  = 4;
    static const uint8_t CTRL_COUNT = 5;

    bool isDnVisible() const { return tempLen >= minLength; }

    uint8_t extendedLen() const {
        // Total entries: controls + palette chars
        // DN is always counted in the index space; scroll logic skips it when hidden
        return CTRL_COUNT + paletteLen;
    }

    bool isCtrlEntry()   const { return charIndex < CTRL_COUNT; }
    bool isOkEntry()     const { return charIndex == CTRL_OK; }
    bool isBcEntry()     const { return charIndex == CTRL_BC; }
    bool isDlEntry()     const { return charIndex == CTRL_DL; }
    bool isSpaceEntry()  const { return charIndex == CTRL_SPC; }
    bool isDnEntry()     const { return charIndex == CTRL_DN; }
    bool isCharEntry()   const { return charIndex >= CTRL_COUNT && charIndex < extendedLen(); }

    // Get the user palette character for the current charIndex
    char currentChar() const {
        if (isCharEntry()) return palette[charIndex - CTRL_COUNT];
        if (isSpaceEntry()) return ' ';
        return '\0';
    }

    // Get user palette index (0-based) from charIndex
    uint8_t paletteIndex() const {
        return (charIndex >= CTRL_COUNT) ? charIndex - CTRL_COUNT : 0;
    }

    // Format the current charIndex entry as a display label
    void formatEntry(char* buf, uint8_t sz) const {
        if (isOkEntry())        snprintf(buf, sz, "OK");
        else if (isBcEntry())   snprintf(buf, sz, "BACK");
        else if (isDlEntry())   snprintf(buf, sz, "DEL");
        else if (isSpaceEntry())snprintf(buf, sz, "SPACE");
        else if (isDnEntry())   snprintf(buf, sz, "DONE");
        else if (isCharEntry()) snprintf(buf, sz, "%c", palette[charIndex - CTRL_COUNT]);
        else                    buf[0] = '\0';
    }

    // ── Scroll through extended palette (Up/Down in char phase) ──────────────
    // Skips DN when it's not visible (tempLen < minLength)
    void scrollUp() {
        uint8_t len = extendedLen();
        do {
            charIndex = (charIndex < len - 1) ? charIndex + 1 : 0;
        } while (charIndex == CTRL_DN && !isDnVisible());
    }

    void scrollDown() {
        uint8_t len = extendedLen();
        do {
            charIndex = (charIndex > 0) ? charIndex - 1 : len - 1;
        } while (charIndex == CTRL_DN && !isDnVisible());
    }

    // ── Select in char phase ─────────────────────────────────────────────────
    //  Returns true if the editor should stay in char phase, false if moved
    //  to action phase.
    bool selectChar() {
        if (isCharEntry() || isSpaceEntry()) {
            // Place current character at editPos
            char ch = currentChar();
            if (editPos < maxLength) {
                // If inserting beyond current length, pad with spaces
                while (tempLen < editPos) {
                    tempBuf[tempLen] = ' ';
                    tempLen++;
                }
                tempBuf[editPos] = ch;
                editPos++;
                if (editPos > tempLen) {
                    tempLen = editPos;
                }
                tempBuf[tempLen] = '\0';
                // If we've reached max length, auto-advance to action phase
                if (editPos >= maxLength) {
                    inActionPhase = true;
                    actionRow     = 3;  // default to Save
                    return false;
                }
                syncCharIndex();
            }
            return true;
        }
        if (isOkEntry()) {
            // Advance to next position (keep existing char at editPos)
            if (editPos < tempLen) {
                editPos++;
                if (editPos >= maxLength) {
                    inActionPhase = true;
                    actionRow     = 3;
                    return false;
                }
                syncCharIndex();
            } else if (editPos < maxLength) {
                // At end of string: extend with space placeholder and advance
                tempBuf[editPos] = ' ';
                editPos++;
                tempLen = editPos;
                tempBuf[tempLen] = '\0';
                if (editPos >= maxLength) {
                    inActionPhase = true;
                    actionRow     = 3;
                    return false;
                }
                syncCharIndex();
            }
            return true;
        }
        if (isBcEntry()) {
            // Go back to previous character position
            if (editPos > 0) {
                editPos--;
                syncCharIndex();
            }
            return true;
        }
        if (isDlEntry()) {
            // Delete character at current position, shift remaining left
            if (editPos < tempLen) {
                for (uint8_t i = editPos; i < tempLen; i++) {
                    tempBuf[i] = tempBuf[i + 1];
                }
                tempLen--;
                tempBuf[tempLen] = '\0';
                // Keep editPos the same (now pointing at next char, or end)
                if (editPos > tempLen && editPos > 0) editPos = tempLen;
                syncCharIndex();
            }
            return true;
        }
        if (isDnEntry()) {
            // Done — go to action phase, keep full string intact
            // (user edited individual chars, rest of string should be preserved)
            inActionPhase = true;
            actionRow     = 3;  // default to Save
            return false;
        }
        return true;
    }

    // ── Action row navigation (Up/Down in action phase) ──────────────────────
    // 5 rows: 0=Redo, 1=Edit, 2=Clear All, 3=Save, 4=Back
    void actionUp() {
        if (actionRow > 0) actionRow--;
        else actionRow = 4;
    }

    void actionDown() {
        if (actionRow < 4) actionRow++;
        else actionRow = 0;
    }

    // ── Length validation ─────────────────────────────────────────────────────
    bool isLengthValid() const {
        return tempLen >= minLength && tempLen <= maxLength;
    }

    // ── Menu list summary: show current stored value ──────────────────────────
    void formatSummary(char* buf, uint8_t sz) const {
        if (valuePtr) {
            strncpy(buf, valuePtr, sz - 1);
            buf[sz - 1] = '\0';
        } else {
            buf[0] = '\0';
        }
    }

private:
    // Sync charIndex to match the character at the current editPos in tempBuf,
    // so the palette scroll starts at the existing character.
    // User chars start at index CTRL_COUNT in the extended palette.
    void syncCharIndex() {
        if (editPos < tempLen) {
            char ch = tempBuf[editPos];
            if (ch == ' ') {
                charIndex = CTRL_SPC;  // space control entry
                return;
            }
            for (uint8_t i = 0; i < paletteLen; i++) {
                if (palette[i] == ch) {
                    charIndex = CTRL_COUNT + i;
                    return;
                }
            }
        }
        // Default: start at first user character (after controls)
        charIndex = (paletteLen > 0) ? CTRL_COUNT : CTRL_OK;
    }
};

#endif // STRING_ADJUSTER_H
