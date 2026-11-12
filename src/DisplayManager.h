// DisplayManager.h
// Owns the U8G2 display pointer, all font pointers, and layout metrics.
// Provides low-level drawing primitives (title bar, separator, scroll bar).
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "MenuTypes.h"

// ── Built-in font presets ─────────────────────────────────────────────────────
// Each preset is a curated trio: { standardFont, titleFont, valueFont }
// that looks balanced together and fits common screen sizes.
//
//  FONT_PRESET_SMALL  – 4x6 / 5x8   / 7x13  → max items on tiny screens
//  FONT_PRESET_NORMAL – 5x8 / 6x12  / 10x20 → balanced default (128x64)
//  FONT_PRESET_LARGE  – 6x12/ 7x14B / 10x20 → readable on large screens
//
// You can also supply any U8g2 font directly via setFonts() or the
// individual setStandardFont() / setTitleFont() / setValueFont() calls.
enum FontPreset {
    FONT_PRESET_SMALL  = 0,
    FONT_PRESET_NORMAL = 1,
    FONT_PRESET_LARGE  = 2
};

class DisplayManager {
public:
    // ── Display pointer ───────────────────────────────────────────────────────
    U8G2* display;

    // ── Screen geometry ───────────────────────────────────────────────────────
    uint16_t screenWidth;
    uint16_t screenHeight;

    // ── Layout metrics (pixels) ───────────────────────────────────────────────
    uint8_t titleHeight;
    uint8_t separatorY;
    uint8_t menuStartY;         // Y of first menu-item row (auto-calculated, no overlap)
    uint8_t lineHeight;
    uint8_t menuItemsVisible;
    uint8_t scrollIndicatorWidth;
    uint8_t menuItemPadding;
    bool    autoItemPaddingEnabled;   // true = recalculate padding on every layout pass

    // ── Display offset ────────────────────────────────────────────────────────
    int16_t offsetX;
    int16_t offsetY;
    bool    useOffset;

    // ── Fonts ─────────────────────────────────────────────────────────────────
    const uint8_t* standardFont;  // menu item rows
    const uint8_t* titleFont;     // menu title bar
    const uint8_t* valueFont;     // large value in numeric editor

    // ── Style flags ───────────────────────────────────────────────────────────
    SelectionStyle selectionStyle;
    bool           showSeparator;
    bool           showScrollBar;

    // ── Blink state (for SELECTION_BLINK) ─────────────────────────────────────
    unsigned long  blinkLastToggle;
    bool           blinkOn;
    unsigned long  blinkInterval;   // ms per half-cycle (default 400)

    // ── Constructor ───────────────────────────────────────────────────────────
    explicit DisplayManager(U8G2* u8g2)
        : display(u8g2),
          screenWidth(128), screenHeight(64),
          titleHeight(12), separatorY(12), menuStartY(14),
          lineHeight(10), menuItemsVisible(4),
          scrollIndicatorWidth(3), menuItemPadding(1), autoItemPaddingEnabled(true),
          offsetX(0), offsetY(0), useOffset(false),
          standardFont(MENU_DEFAULT_STANDARD_FONT),
          titleFont(MENU_DEFAULT_TITLE_FONT),
          valueFont(MENU_DEFAULT_VALUE_FONT),
          selectionStyle(SELECTION_ARROW),
          showSeparator(true),
          showScrollBar(true),
          blinkLastToggle(0), blinkOn(true), blinkInterval(400) {}

    // ── begin(): read screen size then recalculate ────────────────────────────
    void begin() {
        if (display) {
            uint16_t w = display->getDisplayWidth();
            uint16_t h = display->getDisplayHeight();
            if (w > 0 && h > 0) { screenWidth = w; screenHeight = h; }
        }
        recalculateLayout();
    }

    // ── Manual screen size override ───────────────────────────────────────────
    // Only needed if begin() cannot query the display driver directly.
    void setScreenSize(uint16_t w, uint16_t h) {
        if (w > 0) screenWidth  = w;
        if (h > 0) screenHeight = h;
        recalculateLayout();
    }

    // =========================================================================
    // FONT API
    // =========================================================================

    // ── Preset: picks a curated trio and auto-recalculates layout ─────────────
    // Usage:  menu.setFontPreset(FONT_PRESET_SMALL);
    void setFontPreset(FontPreset preset) {
        switch (preset) {
            case FONT_PRESET_SMALL:
                standardFont = u8g2_font_4x6_tr;
                titleFont    = u8g2_font_5x8_tr;
                valueFont    = u8g2_font_7x13_tr;
                break;
            case FONT_PRESET_LARGE:
                standardFont = u8g2_font_6x12_tr;
                titleFont    = u8g2_font_7x14B_tr;
                valueFont    = u8g2_font_10x20_tr;
                break;
            case FONT_PRESET_NORMAL:
            default:
                standardFont = u8g2_font_5x8_tr;
                titleFont    = u8g2_font_6x12_tr;
                valueFont    = u8g2_font_10x20_tr;
                break;
        }
        recalculateLayout();
    }

    // ── Full control: supply all three font roles at once ─────────────────────
    // Usage:  menu.setFonts(u8g2_font_4x6_tr,
    //                       u8g2_font_helvB08_tr,
    //                       u8g2_font_helvB08_tr);
    //
    // Any role can be nullptr to keep its current font unchanged.
    void setFonts(const uint8_t* stdFont,
                  const uint8_t* ttlFont,
                  const uint8_t* valFont) {
        if (stdFont) standardFont = stdFont;
        if (ttlFont) titleFont    = ttlFont;
        if (valFont) valueFont    = valFont;
        recalculateLayout();
    }

    // ── Single-role setters ───────────────────────────────────────────────────
    // Usage:  menu.setStandardFont(u8g2_font_helvB08_tr);
    void setStandardFont(const uint8_t* f) { if (f) { standardFont = f; recalculateLayout(); } }
    void setTitleFont   (const uint8_t* f) { if (f) { titleFont    = f; recalculateLayout(); } }
    void setValueFont   (const uint8_t* f) { if (f) { valueFont    = f; } }   // no layout impact

    // =========================================================================
    // STYLE API
    // =========================================================================
    void setSelectionStyle(SelectionStyle s)  { selectionStyle = s; }
    void setShowSeparator(bool show)          { showSeparator  = show; recalculateLayout(); }
    void setShowScrollBar(bool show)          { showScrollBar  = show; }

    // Blink speed (default 400ms half-cycle = ~1.25 Hz)
    void setBlinkInterval(unsigned long ms)   { blinkInterval = ms; }

    // Call once per render cycle to update blink state
    void updateBlink() {
        unsigned long now = millis();
        if (now - blinkLastToggle >= blinkInterval) {
            blinkOn = !blinkOn;
            blinkLastToggle = now;
        }
    }
    bool isBlinkVisible() const { return blinkOn; }

    // =========================================================================
    // LAYOUT API (manual override)
    // =========================================================================
    // Set explicit inter-row padding in pixels.
    // Call with 255 (or omit) to restore automatic scaling based on font size.
    void setMenuItemPadding(uint8_t p)        { menuItemPadding = p; autoItemPaddingEnabled = false; recalculateLayout(); }
    void setAutoItemPadding()                 { autoItemPaddingEnabled = true; recalculateLayout(); }

    void setLayoutParameters(uint8_t titleH, uint8_t sepY,
                             uint8_t startY, uint8_t lineH) {
        titleHeight = titleH;
        separatorY  = sepY;
        menuStartY  = startY;
        lineHeight  = lineH;
        uint8_t fh  = fontHeight(standardFont);
        menuItemPadding = (lineH > fh) ? (lineH - fh) : 0;
        autoItemPaddingEnabled = false;   // manual override
        int avail = screenHeight - menuStartY;
        menuItemsVisible = (avail / lineHeight < 1) ? 1 : avail / lineHeight;
    }

    void setOffset(int16_t x, int16_t y) { offsetX = x; offsetY = y; useOffset = true; }
    void clearOffset()                   { offsetX = 0; offsetY = 0; useOffset = false; }

    // =========================================================================
    // GEOMETRY HELPERS
    // =========================================================================
    int16_t ox() const { return useOffset ? offsetX : 0; }
    int16_t oy() const { return useOffset ? offsetY : 0; }

    // Line-spacing height: getAscent() - getDescent() gives the typographic
    // cell height the font was designed for — this is the correct row-to-row
    // distance.  getMaxCharHeight() returns only the tallest glyph bitmap
    // which is often shorter than the intended cell height, causing rows to
    // be packed too tightly or inconsistently across font sizes.
    uint8_t fontHeight(const uint8_t* font) {
        display->setFont(font);
        int16_t h = (int16_t)display->getAscent() - (int16_t)display->getDescent();
        return (h > 0) ? (uint8_t)h : display->getMaxCharHeight();
    }

    // Ascent only (pixels above baseline) — used for positioning
    uint8_t fontAscent(const uint8_t* font) {
        display->setFont(font);
        int8_t a = display->getAscent();
        return (a > 0) ? (uint8_t)a : display->getMaxCharHeight();
    }

    uint8_t fontWidth(const uint8_t* font) {
        display->setFont(font);
        return display->getMaxCharWidth();
    }

    // Measure the actual pixel width of the "> " arrow prefix.
    // For monospace fonts this equals fontWidth*2, but for proportional
    // fonts (e.g. Helvetica) getMaxCharWidth returns the widest glyph
    // which overshoots the real "> " width, causing misaligned indents.
    uint8_t arrowIndentWidth(const uint8_t* font) {
        display->setFont(font);
        return (uint8_t)display->getStrWidth("> ");
    }

    // =========================================================================
    // DRAWING PRIMITIVES
    // =========================================================================
    void drawTitleBar(const char* text) {
        display->setFont(titleFont);
        int16_t textW = (int16_t)display->getStrWidth(text);
        int16_t availW = (int16_t)screenWidth;
        int16_t yPos = oy() + titleHeight - 2;

        if (textW <= availW) {
            // Fits — draw normally
            display->setCursor(ox(), yPos);
            display->print(text);
        } else {
            // Auto-scroll (ping-pong)
            uint8_t textLen = (uint8_t)strlen(text);
            uint8_t tfw = fontWidth(titleFont);
            uint8_t fitChars = (availW > 0 && tfw > 0) ? (uint8_t)(availW / tfw) : 1;
            if (fitChars < 1) fitChars = 1;
            if (fitChars > textLen) fitChars = textLen;
            unsigned long now = millis();
            int scrollRange = (int)textLen - (int)fitChars + 1;
            if (scrollRange < 1) scrollRange = 1;
            int scrollPos = (int)((now / 400) % (scrollRange * 2));
            if (scrollPos >= scrollRange) scrollPos = scrollRange * 2 - scrollPos - 1;
            if (scrollPos < 0) scrollPos = 0;
            display->setCursor(ox(), yPos);
            for (uint8_t i = 0; i < fitChars && (scrollPos + i) < textLen; i++)
                display->print(text[scrollPos + i]);
        }
    }

    void drawSeparator() {
        if (!showSeparator) return;
        display->drawHLine(ox(), oy() + separatorY, screenWidth);
    }

    // Inverse-colour highlight box behind a selected row.
    // yPos is the U8G2 text baseline (bottom of glyph).
    // We fill the full row height, then switch font mode so glyphs
    // draw in colour 0 (black) on top of the white box — true inverse.
    void drawSelectionBox(int16_t yPos, uint8_t rowFontH) {
        // Box top = baseline - ascent; add 1px padding above and below
        int16_t boxY = oy() + yPos - (int16_t)rowFontH;
        if (boxY < 0) boxY = 0;
        int16_t boxH = rowFontH + 2;  // ascent + 1px top pad + 1px bottom pad
        display->setDrawColor(1);
        display->drawBox(ox(), boxY, screenWidth, boxH);
        // Transparent font mode: only lit pixels are painted,
        // drawing in colour 0 makes text appear black on the white box.
        display->setFontMode(1);
        display->setDrawColor(0);
    }

    void resetDrawColor() {
        display->setFontMode(0);   // restore opaque font mode
        display->setDrawColor(1);
    }

    // Vertical scroll bar on the right edge
    void drawScrollBar(int cursorPos, int totalItems, int visibleItems) {
        if (!showScrollBar) return;
        if (totalItems <= visibleItems) return;
        int availH = visibleItems * lineHeight;
        int barH   = (visibleItems * availH) / totalItems;
        if (barH < 4) barH = 4;
        int barY = oy() + menuStartY;
        if (totalItems > 1)
            barY += (cursorPos * (availH - barH)) / (totalItems - 1);
        int16_t barX = ox() + screenWidth - scrollIndicatorWidth;
        int16_t topY = oy() + menuStartY;
        display->drawVLine(barX, topY, availH);
        display->drawBox(barX, barY, scrollIndicatorWidth, barH);
    }

private:
    // ── Auto-calculate layout so nothing overlaps ─────────────────────────────
    // Layout areas top→bottom:
    //   [title text]       0 … titleHeight-1
    //   [separator line]   at separatorY   (= titleHeight)
    //   [gap]              small fixed gap
    //   [menu items]       menuStartY … screenHeight-1
    void recalculateLayout() {
        if (!display) return;

        uint8_t stdH     = fontHeight(standardFont);    // ascent + |descent| = proper line pitch
        uint8_t stdAsc   = fontAscent(standardFont);    // ascent only (above baseline)
        uint8_t titleH   = fontHeight(titleFont);

        // ── Title bar height ──────────────────────────────────────────────────
        // Cap at 30% of screen height so the title never eats into menu rows.
        // On very short screens (<=32px) cap at 25%.
        uint8_t maxTitlePct = (screenHeight <= 32) ? 25 : 30;
        uint8_t maxTitleH   = (uint8_t)((uint16_t)screenHeight * maxTitlePct / 100);
        if (maxTitleH < 8) maxTitleH = 8;
        uint8_t rawTitleH = titleH + 1;
        titleHeight = (rawTitleH < maxTitleH) ? rawTitleH : maxTitleH;

        // Separator sits flush below title area
        separatorY = titleHeight;

        // ── menuStartY ────────────────────────────────────────────────────────
        // This is the BASELINE of the first menu row.
        // We need: separatorY + 1 (line pixel) + sepGap (breathing room)
        //          + stdAsc (ascent, so glyph top clears the gap)
        // We use ascent-only here, NOT the full fontHeight, because
        // the baseline already accounts for descent below it.
        uint8_t sepGap = showSeparator ? 2 : 1;
        menuStartY = separatorY + 1 + sepGap + stdAsc;

        // ── Inter-row padding ─────────────────────────────────────────────────
        // In auto mode, padding is always SMALL — the font's built-in
        // line height (ascent + descent) already provides visual separation.
        //   small font (h<=8)  → 2px extra
        //   medium     (h<=12) → 1px extra
        //   large      (h>12)  → 0px extra (font height alone suffices)
        if (autoItemPaddingEnabled) {
            if      (stdH <= 8)  menuItemPadding = 2;
            else if (stdH <= 12) menuItemPadding = 1;
            else                 menuItemPadding = 0;
        }

        // Row height = fontHeight (line pitch) + padding
        // Minimum 1px padding so rows never visually merge
        lineHeight = stdH + menuItemPadding;
        if (lineHeight <= stdH) lineHeight = stdH + 1;

        // How many complete rows fit below menuStartY
        int avail = screenHeight - menuStartY;
        menuItemsVisible = (avail < (int)lineHeight) ? 1 : (uint8_t)(avail / lineHeight);
    }
};

#endif // DISPLAY_MANAGER_H
