// WidgetRenderer.h
// Draws the full-screen editor widgets for each adjuster type.
// Depends on DisplayManager and Adjusters — nothing else.
// Add new widget types here without touching MenuSystem.
#ifndef WIDGET_RENDERER_H
#define WIDGET_RENDERER_H

#include "DisplayManager.h"
#include "Adjusters.h"
#include "MenuTypes.h"
#if MENU_ENABLE_DATE
  #include "DateAdjuster.h"
#endif
#if MENU_ENABLE_TIME
  #include "TimeAdjuster.h"
#endif
#if MENU_ENABLE_GAUGE
  #include "DisplayGauge.h"
#endif
#if MENU_ENABLE_BAR
  #include "DisplayBar.h"
#endif
#if MENU_ENABLE_STRING
  #include "StringAdjuster.h"
#endif

class WidgetRenderer {
public:
    explicit WidgetRenderer(DisplayManager& dm) : dm(dm) {}

    // ── Numeric (float / int) editor ─────────────────────────────────────────
    // Shows large current value, a min–max slider, and unit label.
    void drawNumericEditor(ValueAdjuster* adj, const char* title = "Adjust Value") {
        U8G2* d = dm.display;
        d->clearBuffer();

        // Title bar
        dm.drawTitleBar(title);
        dm.drawSeparator();

        float value    = adj->getValue();
        float minV     = adj->getMin();
        float maxV     = adj->getMax();
        int   decimals = adj->getDecimalPlaces();
        const char* unit = adj->getUnit();

        // Format value string
        char valueStr[20];
        formatFloat(valueStr, sizeof(valueStr), value, decimals);

        // Content zone: from separator to screen bottom
        int16_t contentTop = dm.oy() + dm.menuStartY;
        int16_t contentH   = dm.screenHeight - dm.menuStartY;

        // Choose value font: fall back to standard if valueFont is too tall
        const uint8_t* usedFont = dm.valueFont;
        uint8_t valFH = dm.fontHeight(dm.valueFont);
        if (valFH > (uint8_t)(contentH * 0.45f)) {
            usedFont = dm.standardFont;
            valFH = dm.fontHeight(dm.standardFont);
        }

        // Large centred value
        d->setFont(usedFont);
        int strW    = (int)d->getStrWidth(valueStr);
        int valueY  = contentTop + (int)(contentH * 0.45f);
        int xPos    = dm.ox() + ((int)dm.screenWidth - strW) / 2;
        d->setCursor(xPos, valueY);
        d->print(valueStr);

        // Unit label beside the value
        d->setFont(dm.standardFont);
        uint8_t stdFH = dm.fontHeight(dm.standardFont);
        d->setCursor(xPos + strW + 2, valueY);
        d->print(unit);

        // Slider track (skip if screen too short)
        if (contentH > 20) {
            int sliderW = (int)(dm.screenWidth * 0.85f);
            if (sliderW < 10) sliderW = 10;
            int sliderX = dm.ox() + ((int)dm.screenWidth - sliderW) / 2;
            int sliderY = dm.oy() + (int)(dm.screenHeight * 0.75f);
            d->drawHLine(sliderX, sliderY, sliderW);

            // Slider thumb
            float range = maxV - minV;
            if (range > 0.0f) {
                int thumbX = sliderX + (int)(sliderW * (value - minV) / range);
                int thumbSz = dm.screenWidth > 48 ? 5 : 3;
                d->drawBox(thumbX - thumbSz/2, sliderY - thumbSz/2, thumbSz, thumbSz);
            }

            // Min / max labels (only if they fit below slider)
            int labelY = sliderY + stdFH + 2;
            if (labelY <= dm.oy() + (int)dm.screenHeight) {
                char minStr[10], maxStr[10];
                formatFloat(minStr, sizeof(minStr), minV, decimals);
                formatFloat(maxStr, sizeof(maxStr), maxV, decimals);
                d->setCursor(sliderX, labelY);
                d->print(minStr);
                int maxLabelW = (int)d->getStrWidth(maxStr);
                d->setCursor(sliderX + sliderW - maxLabelW, labelY);
                d->print(maxStr);
            }
        }

        d->sendBuffer();
    }

    // ── Boolean toggle editor ─────────────────────────────────────────────────
    // Shows current stored value and two selectable options.
    void drawBoolEditor(BoolValueAdjuster* adj, const char* title = "Boolean Setting") {
        U8G2* d = dm.display;
        d->clearBuffer();

        dm.drawTitleBar(title);
        d->setFont(dm.standardFont);
        dm.drawSeparator();

        int16_t textX    = dm.ox();
        int     baseY    = dm.separatorY;
        int     h        = dm.screenHeight - baseY;

        int currentValueY  = dm.oy() + baseY + (int)(h * 0.25f);

        // Current stored value
        d->setCursor(textX, currentValueY);
        d->print("Current: ");
        d->print(adj->getCurrentLabel());

        // True / False options — spread evenly in the lower portion
        // Divide remaining space into 3 equal zones; place options at 1/3 and 2/3
        uint8_t boolFh = dm.fontHeight(dm.standardFont);
        int     lowerTop = dm.oy() + baseY + (int)(h * 0.45f);
        int     lowerH   = (dm.oy() + dm.screenHeight) - lowerTop;
        int     opt1Y    = lowerTop + lowerH / 3;
        int     opt2Y    = lowerTop + (lowerH * 2) / 3;

        bool temp = adj->getTempValue();
        drawRow(d, opt1Y, boolFh,  temp, adj->getTrueLabel());
        drawRow(d, opt2Y, boolFh, !temp, adj->getFalseLabel());

        d->sendBuffer();
    }

#if MENU_ENABLE_MULTISELECT
    // ── Multi-select checklist ────────────────────────────────────────────────
    // Scrollable list of [/]/[ ] rows plus a final OK row.
    void drawMultiSelect(MultiSelectAdjuster* adj, const char* title = "Select") {
        U8G2* d = dm.display;
        d->clearBuffer();

        dm.drawTitleBar(title);
        dm.drawSeparator();
        d->setFont(dm.standardFont);
        uint8_t fh        = dm.fontHeight(dm.standardFont);
        uint8_t fw        = dm.fontWidth(dm.standardFont);

        uint8_t rowH      = fh + dm.menuItemPadding;
        uint8_t startY    = dm.oy() + dm.menuStartY;
        uint8_t visOpts   = adj->getVisibleCount();
        uint8_t totalRows = adj->getTotalRows();  // visible options + OK
        uint8_t maxVis    = (dm.screenHeight - dm.menuStartY) / rowH;
        if (maxVis < 1) maxVis = 1;

        uint8_t cur    = adj->getCursorIndex();
        uint8_t scroll = (cur >= maxVis) ? cur - maxVis + 1 : 0;

        if (adj->isEmpty()) {
            // ── No visible options: show [None] + OK ──────────────────────────
            drawRow(d, startY, fh, (cur == 0), "[None]");
            if (maxVis > 1)
                drawRow(d, startY + rowH, fh, (cur >= visOpts), "OK");
        } else {
            // ── Normal list: visible options + OK ─────────────────────────────
            for (uint8_t v = 0; v < maxVis && (scroll + v) < totalRows; v++) {
                uint8_t idx  = scroll + v;
                int16_t rowY = startY + v * rowH;
                bool    hi   = (idx == cur);

                if (idx < visOpts) {
                    // Option row: checkbox + label
                    char rowBuf[MAX_MENU_NAME_LENGTH + 6];
                    snprintf(rowBuf, sizeof(rowBuf), "%s %s",
                             adj->isVisibleSelected(idx) ? "[/]" : "[ ]",
                             adj->getVisibleLabel(idx));

                    // Auto-scroll selected row if text is wider than screen
                    int16_t rowTextW = (int16_t)d->getStrWidth(rowBuf);
                    uint8_t indent = (dm.selectionStyle == SELECTION_ARROW) ? dm.arrowIndentWidth(dm.standardFont) : 2;
                    int16_t availRowW = (int16_t)dm.screenWidth - indent;
                    if (hi && rowTextW > availRowW && availRowW > 0) {
                        // Ping-pong scroll for the selected row
                        uint8_t rowLen = (uint8_t)strlen(rowBuf);
                        uint8_t fitC = (fw > 0) ? (uint8_t)(availRowW / fw) : 1;
                        if (fitC < 1) fitC = 1;
                        if (fitC > rowLen) fitC = rowLen;
                        unsigned long now = millis();
                        int sr = (int)rowLen - (int)fitC + 1;
                        if (sr < 1) sr = 1;
                        int sp = (int)((now / 400) % (sr * 2));
                        if (sp >= sr) sp = sr * 2 - sp - 1;
                        if (sp < 0) sp = 0;
                        // Build a truncated+scrolled buffer
                        char scrolledBuf[MAX_MENU_NAME_LENGTH + 6];
                        uint8_t copyLen = fitC;
                        if (sp + copyLen > rowLen) copyLen = rowLen - sp;
                        memcpy(scrolledBuf, rowBuf + sp, copyLen);
                        scrolledBuf[copyLen] = '\0';
                        drawRow(d, rowY, fh, hi, scrolledBuf);
                    } else {
                        drawRow(d, rowY, fh, hi, rowBuf);
                    }
                } else {
                    // OK row — show validation hint if min not met
                    if (!adj->canConfirm()) {
                        char okBuf[24];
                        snprintf(okBuf, sizeof(okBuf), "OK (min %d)", adj->getMinSelect());
                        drawRow(d, rowY, fh, hi, okBuf);
                    } else {
                        drawRow(d, rowY, fh, hi, "OK");
                    }
                }
            }
        }

        // ── Selection counter in title bar (top-right) ────────────────────────
        if (visOpts > 0) {
            d->setFont(dm.titleFont);
            char cntBuf[12];
            uint8_t maxSel = adj->getMaxSelect();
            if (maxSel > 0)
                snprintf(cntBuf, sizeof(cntBuf), "%d/%d", adj->getSelectedCount(), maxSel);
            else
                snprintf(cntBuf, sizeof(cntBuf), "%d", adj->getSelectedCount());
            int16_t cntW = (int16_t)d->getStrWidth(cntBuf);
            int16_t cntX = dm.ox() + dm.screenWidth - cntW - 1;
            int16_t cntY = dm.oy() + dm.titleHeight - 2;
            d->setCursor(cntX, cntY);
            d->print(cntBuf);
            d->setFont(dm.standardFont);
        }

        // Scroll indicator
        if (totalRows > maxVis) {
            int availH = maxVis * rowH;
            int barH   = (maxVis * availH) / totalRows;
            if (barH < 4) barH = 4;
            int barY   = startY + (cur * (availH - barH)) / (totalRows - 1);
            int16_t barX = dm.ox() + dm.screenWidth - dm.scrollIndicatorWidth;
            d->drawVLine(barX, startY, availH);
            d->drawBox(barX, barY, dm.scrollIndicatorWidth, barH);
        }

        d->sendBuffer();
    }


#endif // MENU_ENABLE_MULTISELECT

#if MENU_ENABLE_DATE
    // ── Date editor ──────────────────────────────────────────────────────────
    // The ">" marker lives inside the date row and moves field by field.
    // After the last field, OK steps down to Redo / Set / Back.
    //
    //   Title
    //   ─────────────────────────────
    //   > 18  | Aug  | 2021          ← field 0 active
    //     Redo
    //     Set
    //   < Back
    //
    //   Title
    //   ─────────────────────────────
    //   18 | > April | 2021          ← field 1 active
    //     Redo
    //     Set
    //   < Back
    //
    //   Title
    //   ─────────────────────────────
    //   18 | April | 2021
    //   > Redo                       ← Redo active  (resets temp, restarts)
    //     Set
    //   < Back
    //
    // activeCol: 0..numCols-1 = fields, numCols = Redo, +1 = Set, +2 = Back
    //
    void drawDateEditor(DateAdjuster* adj, const char* title = "Set Date") {
        U8G2* d = dm.display;
        d->clearBuffer();

        d->setFont(dm.standardFont);

        uint8_t fh   = dm.fontHeight(dm.standardFont);
        uint8_t fw   = dm.fontWidth(dm.standardFont);
        uint8_t rowH = fh + dm.menuItemPadding;
        if (rowH <= fh) rowH = fh + 1;

        // We need 5 rows total: date row + Redo + Edit + Save + Back
        int16_t totalNeeded = rowH * 5;
        int16_t baseY;

        // Check if everything fits with title bar
        if (dm.oy() + dm.menuStartY + totalNeeded <= dm.oy() + dm.screenHeight) {
            // Fits with title — draw normally
            dm.drawTitleBar(title);
            dm.drawSeparator();
            d->setFont(dm.standardFont);
            baseY = dm.oy() + dm.menuStartY;
        } else {
            // Doesn't fit — skip title and separator, start from top
            baseY = dm.oy() + fh + 1;
            // Try to reduce rowH to minimum (just fh+1) if still doesn't fit
            if (baseY + rowH * 5 > dm.oy() + dm.screenHeight) {
                rowH = fh + 1;
            }
        }

        uint8_t  nc    = adj->numCols;

        // ── Arrow prefix width (used to indent non-arrow rows too) ────────────
        // Use measured "> " width for proportional font accuracy.
        uint8_t arrowPfx = (dm.selectionStyle == SELECTION_ARROW) ? dm.arrowIndentWidth(dm.standardFont) : 0;

        // ─────────────────────────────────────────────────────────────────────
        // ROW 0: date row
        // All fields and "|" separators are on one line.
        // The active-field ">" is printed inline before that field only.
        // Column x positions are pre-calculated with a fixed max width per
        // field so they never shift as the value changes (e.g. "8" vs "12").
        // ─────────────────────────────────────────────────────────────────────
        {
            int16_t yPos   = baseY;
            bool    anyFld = adj->isOnField();

            // Pre-compute fixed pixel width for each field (max possible string)
            uint8_t fixW[3];
            for (uint8_t c = 0; c < nc; c++) {
                DateField f = (DateField)adj->fieldSequence[c];
                fixW[c] = adj->fieldWidth(f) * fw;
                // Measure the actual current string too — use the larger
                char buf[12]; adj->formatField(f, buf, sizeof(buf));
                uint8_t actual = d->getStrWidth(buf);
                if (actual > fixW[c]) fixW[c] = actual;
            }

            // Calculate total row width
            int16_t totalRowW = 0;
            for (uint8_t c = 0; c < nc; c++) {
                totalRowW += arrowPfx + fixW[c];
                if (c < nc - 1) totalRowW += fw;  // "|" separator
            }

            // Horizontal scroll offset — if total row exceeds screen width,
            // shift left so the active field is visible and centred
            int16_t hScroll = 0;
            if (totalRowW > (int16_t)dm.screenWidth) {
                // Find active column centre position
                int16_t activeCenter = 0;
                int16_t px = 0;
                for (uint8_t c = 0; c < nc; c++) {
                    px += arrowPfx;
                    if (anyFld && c == adj->activeCol)
                        activeCenter = px + fixW[c] / 2;
                    px += fixW[c];
                    if (c < nc - 1) px += fw;
                }
                hScroll = activeCenter - (int16_t)dm.screenWidth / 2;
                if (hScroll < 0) hScroll = 0;
                if (hScroll > totalRowW - (int16_t)dm.screenWidth)
                    hScroll = totalRowW - (int16_t)dm.screenWidth;
            }

            // Walk through columns and determine x of each
            int16_t colX[3];
            int16_t cx = dm.ox() - hScroll;
            for (uint8_t c = 0; c < nc; c++) {
                cx += arrowPfx;
                colX[c] = cx;
                cx += fixW[c];
                if (c < nc - 1) cx += fw;
            }

            // Draw the row — per-field highlight for each selection style
            for (uint8_t c = 0; c < nc; c++) {
                DateField f    = (DateField)adj->fieldSequence[c];
                bool      isSel = anyFld && (c == adj->activeCol);
                char      buf[12];
                adj->formatField(f, buf, sizeof(buf));

                // Skip fields that are entirely off-screen
                if (colX[c] + (int16_t)fixW[c] < dm.ox() || colX[c] > dm.ox() + (int16_t)dm.screenWidth)
                    continue;

                if (isSel) {
                    drawFieldHighlight(d, colX[c], yPos, fh, arrowPfx, buf);
                } else {
                    d->setCursor(colX[c], yPos);
                    d->print(buf);
                }

                if (c < nc - 1) {
                    int16_t sepX = colX[c] + fixW[c] + fw / 4;
                    if (sepX >= dm.ox() && sepX < dm.ox() + (int16_t)dm.screenWidth) {
                        d->setCursor(sepX, yPos);
                        d->print("|");
                    }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // ROW 1–4: Redo / Edit / Save / < Back (scrollable if not all fit)
        // ─────────────────────────────────────────────────────────────────────
        {
            static const char* const actLabels[4] = { "Redo", "Edit", "Save", "< Back" };
            const bool actSel[4] = { adj->isOnRedo(), adj->isOnEdit(), adj->isOnSave(), adj->isOnBack() };
            drawActionRows(d, baseY + rowH, fh, rowH, actLabels, actSel, 4);
        }

        d->sendBuffer();
    }

#endif // MENU_ENABLE_DATE

#if MENU_ENABLE_TIME
    // ── Time editor ──────────────────────────────────────────────────────────
    // Identical chrome to drawDateEditor but with time semantics and am/pm.
    //
    //   Title
    //   ──────────────────────────────────────
    //   > 13 | 32 | 43           (24h, HH active)
    //     Redo
    //     Set
    //   < Back
    //
    //   Title
    //   ──────────────────────────────────────
    //   01 | > 32 | pm           (12h, mm active)
    //     Redo
    //     Set
    //   < Back
    //
    void drawTimeEditor(TimeAdjuster* adj, const char* title = "Set Time") {
        U8G2* d = dm.display;
        d->clearBuffer();

        d->setFont(dm.standardFont);

        uint8_t fh   = dm.fontHeight(dm.standardFont);
        uint8_t fw   = dm.fontWidth(dm.standardFont);
        uint8_t rowH = fh + dm.menuItemPadding;
        if (rowH <= fh) rowH = fh + 1;

        // We need 5 rows total: time row + Redo + Edit + Save + Back
        int16_t totalNeeded = rowH * 5;
        int16_t baseY;

        // Check if everything fits with title bar
        if (dm.oy() + dm.menuStartY + totalNeeded <= dm.oy() + dm.screenHeight) {
            // Fits with title — draw normally
            dm.drawTitleBar(title);
            dm.drawSeparator();
            d->setFont(dm.standardFont);
            baseY = dm.oy() + dm.menuStartY;
        } else {
            // Doesn't fit — skip title and separator, start from top
            baseY = dm.oy() + fh + 1;
            // Try to reduce rowH to minimum (just fh+1) if still doesn't fit
            if (baseY + rowH * 5 > dm.oy() + dm.screenHeight) {
                rowH = fh + 1;
            }
        }

        uint8_t  nc    = adj->numCols;

        uint8_t arrowPfx = (dm.selectionStyle == SELECTION_ARROW) ? dm.arrowIndentWidth(dm.standardFont) : 0;

        // ── Time row (auto-scrolls horizontally when too wide) ──────────────
        {
            int16_t yPos   = baseY;
            bool    anyFld = adj->isOnField();

            // Pre-compute fixed pixel width per column
            uint8_t fixW[4];
            for (uint8_t c = 0; c < nc; c++) {
                TimeField f = (TimeField)adj->fieldSequence[c];
                fixW[c] = adj->fieldWidth(f) * fw;
                char buf[8]; adj->formatField(f, buf, sizeof(buf));
                uint8_t actual = d->getStrWidth(buf);
                if (actual > fixW[c]) fixW[c] = actual;
            }

            // Calculate total row width
            int16_t totalRowW = 0;
            for (uint8_t c = 0; c < nc; c++) {
                totalRowW += arrowPfx + fixW[c];
                if (c < nc - 1) totalRowW += fw;
            }

            // Horizontal scroll offset — centre on active field if row too wide
            int16_t hScroll = 0;
            if (totalRowW > (int16_t)dm.screenWidth) {
                int16_t activeCenter = 0;
                int16_t px = 0;
                for (uint8_t c = 0; c < nc; c++) {
                    px += arrowPfx;
                    if (anyFld && c == adj->activeCol)
                        activeCenter = px + fixW[c] / 2;
                    px += fixW[c];
                    if (c < nc - 1) px += fw;
                }
                hScroll = activeCenter - (int16_t)dm.screenWidth / 2;
                if (hScroll < 0) hScroll = 0;
                if (hScroll > totalRowW - (int16_t)dm.screenWidth)
                    hScroll = totalRowW - (int16_t)dm.screenWidth;
            }

            // Calculate column x positions with scroll offset
            int16_t colX[4];
            int16_t cx = dm.ox() - hScroll;
            for (uint8_t c = 0; c < nc; c++) {
                cx += arrowPfx;
                colX[c] = cx;
                cx += fixW[c];
                if (c < nc - 1) cx += fw;
            }

            // Draw the row — per-field highlight for each selection style
            for (uint8_t c = 0; c < nc; c++) {
                TimeField f     = (TimeField)adj->fieldSequence[c];
                bool      isSel = anyFld && (c == adj->activeCol);
                char      buf[8];
                adj->formatField(f, buf, sizeof(buf));

                // Skip fields entirely off-screen
                if (colX[c] + (int16_t)fixW[c] < dm.ox() || colX[c] > dm.ox() + (int16_t)dm.screenWidth)
                    continue;

                if (isSel) {
                    drawFieldHighlight(d, colX[c], yPos, fh, arrowPfx, buf);
                } else {
                    d->setCursor(colX[c], yPos);
                    d->print(buf);
                }

                // "|" separator between fields
                if (c < nc - 1) {
                    int16_t sepX = colX[c] + fixW[c] + fw / 4;
                    if (sepX >= dm.ox() && sepX < dm.ox() + (int16_t)dm.screenWidth) {
                        d->setCursor(sepX, yPos);
                        d->print("|");
                    }
                }
            }
        }

        // ── Action rows (scrollable if not all fit) ────────────────────────
        {
            static const char* const actLabels[4] = { "Redo", "Edit", "Save", "< Back" };
            const bool actSel[4] = { adj->isOnRedo(), adj->isOnEdit(), adj->isOnSave(), adj->isOnBack() };
            drawActionRows(d, baseY + rowH, fh, rowH, actLabels, actSel, 4);
        }

        d->sendBuffer();
    }


#endif // MENU_ENABLE_TIME

#if MENU_ENABLE_STRING
    // ── String editor ────────────────────────────────────────────────────────
    //
    //  Phase 1 (character input):
    //  ┌──────────────────────────────┐
    //  │  Name                        │  ← title bar
    //  │ ──────────────────────────── │
    //  │  H e l l [o] _ _ _          │  ← string with active pos highlighted
    //  │       ↕ o                    │  ← current char from palette
    //  │    OK  [ ]  DN               │  ← special entries
    //  └──────────────────────────────┘
    //
    //  Phase 2 (action rows — after DN):
    //  ┌──────────────────────────────┐
    //  │  Name                        │
    //  │ ──────────────────────────── │
    //  │  "Hello"              5/16   │  ← preview + length
    //  │    Redo                       │
    //  │  > Ok                        │
    //  │    Back                       │
    //  └──────────────────────────────┘
    //
    void drawStringEditor(StringAdjuster* adj, const char* title = "Edit String") {
        U8G2* d = dm.display;
        d->clearBuffer();

        d->setFont(dm.standardFont);

        uint8_t fh   = dm.fontHeight(dm.standardFont);
        uint8_t fw   = dm.fontWidth(dm.standardFont);
        uint8_t rowH = fh + dm.menuItemPadding;
        if (rowH <= fh) rowH = fh + 1;

        int16_t baseY;

        if (adj->isOnConfirm()) {
            // Clear All confirm dialog phase
            dm.drawTitleBar(title);
            dm.drawSeparator();
            d->setFont(dm.standardFont);
            baseY = dm.oy() + dm.menuStartY;
            drawStringConfirmClear(d, adj, baseY, fh, rowH);
        } else if (adj->isOnChar()) {
            // Char phase — always draw title
            dm.drawTitleBar(title);
            dm.drawSeparator();
            d->setFont(dm.standardFont);
            baseY = dm.oy() + dm.menuStartY;
            drawStringCharPhase(d, adj, baseY, fh, fw, rowH);
        } else {
            // ── Action phase: 4-level fit cascade ────────────────────────────
            // Full layout needs:
            //   title+sep  → menuStartY rows above baseY
            //   preview    → rowH
            //   charcount  → rowH
            //   5 actions  → rowH * 5
            // Total with title:    menuStartY + rowH * 7
            // Total without title: rowH * 7  (baseY = oy + rowH)
            //
            // Level 0: fits with title
            // Level 1: drop title, start from top (7 rows)
            // Level 2: drop charcount row too (compact, 6 rows)
            // Level 3: single-line action bar (2 rows: preview + bar)

            int16_t screenTop    = dm.oy();
            int16_t screenBottom = dm.oy() + dm.screenHeight;

            // Level 0: with title
            int16_t baseWithTitle = (int16_t)(dm.oy() + dm.menuStartY);
            bool fitsL0 = (baseWithTitle + rowH * 7 <= screenBottom);

            // Level 1: no title, baseY = first text row from top
            int16_t baseNoTitle = screenTop + fh + 1;
            bool fitsL1 = (baseNoTitle + rowH * 7 <= screenBottom);

            // Level 2: no title + compact (charcount merged into preview = 6 rows)
            bool fitsL2 = (baseNoTitle + rowH * 6 <= screenBottom);

            // Level 3: single-line action bar (preview row + 1 bar row = 2 rows)
            // (always fits as long as 2*rowH <= availH)

            uint8_t fitLevel;
            if      (fitsL0) { fitLevel = 0; baseY = baseWithTitle; }
            else if (fitsL1) { fitLevel = 1; baseY = baseNoTitle; }
            else if (fitsL2) { fitLevel = 2; baseY = baseNoTitle; }
            else             { fitLevel = 3; baseY = screenTop + fh + 1; }

            if (fitLevel == 0) {
                dm.drawTitleBar(title);
                dm.drawSeparator();
                d->setFont(dm.standardFont);
            }

            drawStringActionPhase(d, adj, baseY, fh, fw, rowH, fitLevel);
        }

        d->sendBuffer();
    }

private:
    // ── String editor: character input phase ──────────────────────────────────
    //
    //  The available vertical space (baseY → screen bottom) is divided into
    //  4 equal slots:
    //    Slot 0: string overview (active char highlighted)
    //    Slot 1: neighbour preview  A > B < C  (big font for selected)
    //    Slot 2: Pos: x/y | min-max
    //    Slot 3: ↑↓ scroll hint  (hidden if slot too thin)
    //
    void drawStringCharPhase(U8G2* d, StringAdjuster* adj,
                             int16_t baseY, uint8_t fh, uint8_t fw, uint8_t rowH) {

        int16_t screenBottom = dm.oy() + (int16_t)dm.screenHeight;
        int16_t totalH       = screenBottom - baseY;
        if (totalH < 4) return;   // nothing fits

        // Slot 0 (string overview) sits immediately at baseY, baseline = baseY + fh.
        // The remaining space below is split into 3 slots for: selector, pos, hint.
        int16_t y0 = baseY + (int16_t)fh;   // string overview baseline — right under the separator

        int16_t remainH = screenBottom - y0; // space left for the other 3 rows
        int16_t slotH   = remainH / 3;
        if (slotH < (int16_t)fh + 1) slotH = (int16_t)fh + 1;

        int16_t y1 = y0 + slotH;            // neighbour preview baseline
        int16_t y2 = y1 + slotH;            // pos/counter baseline
        int16_t y3 = screenBottom - 1;       // scroll hint: pinned to very bottom

        // ── Slot 0: string overview ───────────────────────────────────────────
        {
            int16_t xCur = dm.ox() + 1;
            uint8_t charsPerScreen = (dm.screenWidth - 2) / (fw + 1);
            uint8_t dispStart = 0;
            if (adj->maxLength > charsPerScreen && adj->editPos >= charsPerScreen) {
                dispStart = adj->editPos - charsPerScreen + 1;
            }
            uint8_t dispEnd = dispStart + charsPerScreen;
            if (dispEnd > adj->maxLength) dispEnd = adj->maxLength;

            d->setFont(dm.standardFont);
            for (uint8_t i = dispStart; i < dispEnd; i++) {
                char ch = (i < adj->tempLen) ? adj->tempBuf[i] : '_';
                bool isActive = (i == adj->editPos);

                if (isActive) {
                    char selCh;
                    if (adj->isCharEntry() || adj->isSpaceEntry()) selCh = adj->currentChar();
                    else selCh = (i < adj->tempLen) ? adj->tempBuf[i] : '_';

                    switch (dm.selectionStyle) {
                        case SELECTION_INVERSE: {
                            int16_t boxY = y0 - (int16_t)fh;
                            if (boxY < 0) boxY = 0;
                            d->setDrawColor(1);
                            d->drawBox(xCur - 1, boxY, fw + 2, fh + 2);
                            d->setFontMode(1);
                            d->setDrawColor(0);
                            d->setCursor(xCur, y0);
                            d->print(selCh);
                            dm.resetDrawColor();
                            break;
                        }
                        case SELECTION_BRACKET:
                            d->setCursor(xCur, y0);
                            d->print('['); d->print(selCh); d->print(']');
                            xCur += fw * 2;
                            break;
                        case SELECTION_BLINK:
                            if (dm.isBlinkVisible()) { d->setCursor(xCur, y0); d->print(selCh); }
                            break;
                        case SELECTION_NONE:
                        case SELECTION_ARROW:
                        default:
                            d->setCursor(xCur, y0); d->print(selCh);
                            break;
                    }
                } else {
                    d->setCursor(xCur, y0);
                    d->print(ch);
                }
                xCur += fw + 1;
            }
        }

        // ── Slot 1: neighbour preview  prev > SEL < next ──────────────────────
        {
            char entryBuf[8];
            adj->formatEntry(entryBuf, sizeof(entryBuf));

            // Choose big font; fall back to standard if it won't fit in slotH
            const uint8_t* bigFont = dm.titleFont;
            uint8_t bigFh = dm.fontHeight(bigFont);
            if (bigFh > (uint8_t)slotH) {
                bigFont = dm.standardFont;
                bigFh   = fh;
            }

            // On very narrow screens, simplify the selector display
            bool showNeighbours = (dm.screenWidth >= 48);

            if (!showNeighbours) {
                // ── Minimal mode: just show centred current selection ──────────
                d->setFont(bigFont);
                int16_t selW = (int16_t)d->getStrWidth(entryBuf);
                int16_t cx = dm.ox() + ((int16_t)dm.screenWidth - selW) / 2;
                if (cx < dm.ox()) cx = dm.ox();
                d->setCursor(cx, y1);
                d->print(entryBuf);
                d->setFont(dm.standardFont);
            } else {
                // ── Full mode: prev > SEL < next ──────────────────────────────
                // Measure widths (standard font for everything first)
                d->setFont(dm.standardFont);
                int16_t arrowW = (int16_t)d->getStrWidth(">");
                int16_t gapPx  = 3;

                // Peek prev/next palette indices without touching adj state
                uint8_t extLen  = adj->extendedLen();
                uint8_t nextIdx = adj->charIndex;
                {
                    uint8_t tries = extLen;
                    do {
                        nextIdx = (nextIdx < extLen - 1) ? nextIdx + 1 : 0;
                        tries--;
                    } while (nextIdx == StringAdjuster::CTRL_DN && !adj->isDnVisible() && tries > 0);
                }
                uint8_t prevIdx = adj->charIndex;
                {
                    uint8_t tries = extLen;
                    do {
                        prevIdx = (prevIdx > 0) ? prevIdx - 1 : extLen - 1;
                        tries--;
                    } while (prevIdx == StringAdjuster::CTRL_DN && !adj->isDnVisible() && tries > 0);
                }

                char prevBuf[8], nextBuf[8];
                auto fmtIdx = [&](uint8_t idx, char* buf, uint8_t sz) {
                    if      (idx == StringAdjuster::CTRL_OK)  snprintf(buf, sz, "OK");
                    else if (idx == StringAdjuster::CTRL_BC)  snprintf(buf, sz, "BC");
                    else if (idx == StringAdjuster::CTRL_DL)  snprintf(buf, sz, "DL");
                    else if (idx == StringAdjuster::CTRL_SPC) snprintf(buf, sz, "[ ]");
                    else if (idx == StringAdjuster::CTRL_DN)  snprintf(buf, sz, "DN");
                    else if (idx >= StringAdjuster::CTRL_COUNT && idx < adj->extendedLen())
                        snprintf(buf, sz, "%c", adj->palette[idx - StringAdjuster::CTRL_COUNT]);
                    else buf[0] = '\0';
                };
                fmtIdx(prevIdx, prevBuf, sizeof(prevBuf));
                fmtIdx(nextIdx, nextBuf, sizeof(nextBuf));

                int16_t prevW = (int16_t)d->getStrWidth(prevBuf);
                int16_t nextW = (int16_t)d->getStrWidth(nextBuf);

                // Selected char width in big font
                d->setFont(bigFont);
                int16_t selW = (int16_t)d->getStrWidth(entryBuf);
                d->setFont(dm.standardFont);

                // Total row width and centred start X
                int16_t totalW = prevW + gapPx + arrowW + gapPx + selW + gapPx + arrowW + gapPx + nextW;
                int16_t cx = dm.ox() + ((int16_t)dm.screenWidth - totalW) / 2;
                if (cx < dm.ox()) cx = dm.ox();

                // All items share the same baseline y1.
                int16_t stdY = y1 + ((int16_t)bigFh - (int16_t)fh) / 2;

                // prev (standard)
                d->setFont(dm.standardFont);
                d->setCursor(cx, stdY); d->print(prevBuf);
                cx += prevW + gapPx;

                // ">" (standard)
                d->setCursor(cx, stdY); d->print(">");
                cx += arrowW + gapPx;

                // selected (big font)
                d->setFont(bigFont);
                d->setCursor(cx, y1); d->print(entryBuf);
                cx += selW + gapPx;

                // "<" (standard)
                d->setFont(dm.standardFont);
                d->setCursor(cx, stdY); d->print("<");
                cx += arrowW + gapPx;

                // next (standard)
                d->setCursor(cx, stdY); d->print(nextBuf);

                d->setFont(dm.standardFont);
            }
        }

        // ── Slot 2: Pos + Min-Max (auto-scroll if too wide) ───────────────
        {
            char infoBuf[28];
            snprintf(infoBuf, sizeof(infoBuf), "Pos:%d/%d | %d-%d",
                     adj->editPos + 1, adj->maxLength,
                     adj->minLength, adj->maxLength);
            int16_t infoPixW = (int16_t)d->getStrWidth(infoBuf);
            int16_t availInfoW = (int16_t)dm.screenWidth - 2;
            if (infoPixW <= availInfoW) {
                d->setCursor(dm.ox() + 1, y2);
                d->print(infoBuf);
            } else {
                uint8_t infoLen = (uint8_t)strlen(infoBuf);
                uint8_t fitC = (availInfoW > 0 && fw > 0) ? (uint8_t)(availInfoW / fw) : 1;
                if (fitC < 1) fitC = 1;
                if (fitC > infoLen) fitC = infoLen;
                unsigned long now = millis();
                int sr = (int)infoLen - (int)fitC + 1;
                if (sr < 1) sr = 1;
                int sp = (int)((now / 400) % (sr * 2));
                if (sp >= sr) sp = sr * 2 - sp - 1;
                if (sp < 0) sp = 0;
                d->setCursor(dm.ox() + 1, y2);
                for (uint8_t i = 0; i < fitC && (sp + i) < infoLen; i++)
                    d->print(infoBuf[sp + i]);
            }
        }

        // ── Slot 3: scroll hint (only if space between y2 and bottom) ─────────
        if (y3 > y2 + (int16_t)fh) {
            d->setCursor(dm.ox(), y3);
            d->print("\x18\x19 scroll");
        }
    }

    // ── String editor: action phase ──────────────────────────────────────────
    //
    //  fitLevel 0/1 (full):
    //    "Preview"   [scrolls if long]
    //    12/32 chars
    //    Redo
    //    Edit
    //    Clear All
    //    Save
    //    < Back
    //
    //  fitLevel 2 (compact — charcount merged into preview row):
    //    "Preview" 12/32
    //    Redo
    //    Edit
    //    Clear All
    //    Save
    //    < Back
    //
    //  fitLevel 3 (inline bar — all 5 actions on one horizontal line):
    //    "Preview" 12/32
    //    Redo | Edit | ClrAll | Save | Back
    //    (selected item highlighted, others plain)
    //
    void drawStringActionPhase(U8G2* d, StringAdjuster* adj,
                               int16_t baseY, uint8_t fh, uint8_t fw,
                               uint8_t rowH, uint8_t fitLevel) {

        bool mergeCount = (fitLevel >= 2);  // charcount merged into preview
        bool inlineBar  = (fitLevel >= 3);  // all actions on one line

        // ── Row 0: preview string ─────────────────────────────────────────────
        int16_t row0Y = baseY;
        {
            char previewBuf[MAX_STRING_LENGTH + 20];
            if (mergeCount) {
                snprintf(previewBuf, sizeof(previewBuf), "\"%s\" %d/%d",
                         adj->tempBuf, adj->tempLen, adj->maxLength);
            } else {
                snprintf(previewBuf, sizeof(previewBuf), "\"%s\"", adj->tempBuf);
            }

            int16_t previewPixW = (int16_t)d->getStrWidth(previewBuf);
            int16_t availW      = (int16_t)dm.screenWidth - 2;

            if (previewPixW <= availW) {
                d->setCursor(dm.ox() + 1, row0Y);
                d->print(previewBuf);
            } else {
                // Auto-scroll (ping-pong)
                uint8_t  previewLen = (uint8_t)strlen(previewBuf);
                uint8_t  fitChars   = 0;
                int16_t  accW       = 0;
                for (uint8_t i = 0; i < previewLen; i++) {
                    char tmp[2] = { previewBuf[i], '\0' };
                    accW += (int16_t)d->getStrWidth(tmp);
                    if (accW > availW) break;
                    fitChars++;
                }
                if (fitChars < 1) fitChars = 1;
                unsigned long now        = millis();
                int           scrollRange = previewLen - fitChars + 1;
                if (scrollRange < 1) scrollRange = 1;
                int scrollPos = (int)((now / 400) % (scrollRange * 2));
                if (scrollPos >= scrollRange) scrollPos = scrollRange * 2 - scrollPos - 1;
                if (scrollPos < 0) scrollPos = 0;
                d->setCursor(dm.ox() + 1, row0Y);
                for (uint8_t i = 0; i < fitChars && (scrollPos + i) < previewLen; i++)
                    d->print(previewBuf[scrollPos + i]);
            }
        }

        // ── Row 1 (full only): char count ─────────────────────────────────────
        int16_t actStart;
        if (!mergeCount) {
            int16_t row1Y = baseY + rowH;
            char lenBuf[24];
            snprintf(lenBuf, sizeof(lenBuf), "%d/%d chars", adj->tempLen, adj->maxLength);
            d->setCursor(dm.ox() + 1, row1Y);
            d->print(lenBuf);
            if (adj->tempLen < adj->minLength) {
                char wBuf[16];
                snprintf(wBuf, sizeof(wBuf), " (min %d)", adj->minLength);
                d->print(wBuf);
            }
            actStart = baseY + rowH * 2;
        } else {
            actStart = baseY + rowH;
        }

        // ── Action rows ───────────────────────────────────────────────────────
        if (!inlineBar) {
            // Scrollable stacked rows
            static const char* const actLabels[5] = { "Redo", "Edit", "Clear All", "Save", "< Back" };
            const bool actSel[5] = {
                adj->isOnRedo(), adj->isOnEdit(), adj->isOnClearAll(),
                adj->isOnSave(), adj->isOnBack()
            };
            drawActionRows(d, actStart, fh, rowH, actLabels, actSel, 5);
        } else {
            // ── Inline bar: Redo | Edit | Clr | Save | Back ────────────────
            // Labels (shortened to save width on tiny screens)
            static const char* const labels[5] = { "Redo", "Edit", "Clr", "Save", "Back" };
            const bool sel[5] = {
                adj->isOnRedo(), adj->isOnEdit(), adj->isOnClearAll(),
                adj->isOnSave(), adj->isOnBack()
            };

            const uint8_t N    = 5;
            const uint8_t padX = 2;   // px padding each side of a label
            uint8_t sepW = (uint8_t)d->getStrWidth("|");
            if (sepW < 1) sepW = 1;

            // Measure each label
            uint8_t labelW[N];
            uint16_t totalW = 0;
            for (uint8_t i = 0; i < N; i++) {
                labelW[i] = (uint8_t)d->getStrWidth(labels[i]);
                totalW   += (uint16_t)(labelW[i] + padX * 2);
            }
            totalW += (uint16_t)(sepW * (N - 1));

            // Centre the bar, or left-align if too wide
            int16_t cx = dm.ox();
            if ((int16_t)totalW < (int16_t)dm.screenWidth) {
                cx = dm.ox() + ((int16_t)dm.screenWidth - (int16_t)totalW) / 2;
            }

            int16_t barY = actStart;

            for (uint8_t i = 0; i < N; i++) {
                int16_t itemX = cx + (int16_t)padX;   // left edge of label text

                if (sel[i]) {
                    switch (dm.selectionStyle) {
                        case SELECTION_INVERSE: {
                            int16_t boxY = barY - (int16_t)fh;
                            if (boxY < 0) boxY = 0;
                            d->setDrawColor(1);
                            d->drawBox(cx, boxY, (int16_t)(labelW[i] + padX * 2), fh + 2);
                            d->setFontMode(1);
                            d->setDrawColor(0);
                            d->setCursor(itemX, barY);
                            d->print(labels[i]);
                            dm.resetDrawColor();
                            break;
                        }
                        case SELECTION_BRACKET:
                            d->setCursor(cx, barY);
                            d->print("[");
                            d->print(labels[i]);
                            d->print("]");
                            break;
                        case SELECTION_BLINK:
                            if (dm.isBlinkVisible()) {
                                d->setCursor(itemX, barY);
                                d->print(labels[i]);
                            }
                            break;
                        case SELECTION_NONE:
                        case SELECTION_ARROW:
                        default:
                            // Arrow style: prefix ">" before selected label
                            d->setCursor(cx, barY);
                            d->print(">");
                            d->print(labels[i]);
                            break;
                    }
                } else {
                    d->setCursor(itemX, barY);
                    d->print(labels[i]);
                }

                // Advance x past this item
                cx += (int16_t)(labelW[i] + padX * 2);

                // Draw separator and advance past it
                if (i < N - 1) {
                    d->setCursor(cx, barY);
                    d->print("|");
                    cx += (int16_t)sepW;
                }
            }
        }
    }

    // ── String editor: Clear All confirm dialog ────────────────────────────────
    //  Title
    //  ──────────────────────
    //  Clear All?
    //  > Yes
    //    No
    void drawStringConfirmClear(U8G2* d, StringAdjuster* adj,
                                int16_t baseY, uint8_t fh, uint8_t rowH) {
        // Prompt row
        d->setCursor(dm.ox() + 1, baseY);
        d->print("Clear All?");

        // Yes / No rows
        drawRow(d, baseY + rowH,     fh, adj->confirmIsYes(), "Yes");
        drawRow(d, baseY + rowH * 2, fh, adj->confirmIsNo(),  "No");
    }

#endif // MENU_ENABLE_STRING

public:

#if MENU_ENABLE_BAR
    // ── Bar graph (full-screen default view) ──────────────────────────────────
    //
    //  BAR_DEFAULT layout:
    //  ┌──────────────────────────────┐
    //  │  Title                  2/5  │  ← title bar + optional counter
    //  │ ──────────────────────────── │  ← separator
    //  │  Subtitle                    │  ← item name
    //  │          70%                 │  ← large centred value (or "70/100 %")
    //  │  ┌█████████████───────┐     │  ← horizontal bar
    //  │  0                   100    │  ← min/max labels
    //  └──────────────────────────────┘
    //
    //  BAR_COMPACT layout:
    //  ┌──────────────────────────────┐
    //  │  Title                  2/5  │
    //  │ ──────────────────────────── │
    //  │  Temperature          35C   │  ← name + value on one row
    //  │  -20 ┌████████──┐      50   │  ← min + bar + max on next row
    //  └──────────────────────────────┘
    //
    //  If showMinMax=false in compact, bar extends full width.
    //
    void drawBarScreen(DisplayBarBase* b, const char* title    = "",
                                          const char* itemName = "",
                                          int counterCurrent = 0,
                                          int counterTotal   = 0) {
        U8G2* d = dm.display;
        d->clearBuffer();

        const int16_t ox = dm.ox();
        const int16_t oy = dm.oy();
        const int16_t W  = (int16_t)dm.screenWidth;
        const int16_t H  = (int16_t)dm.screenHeight;

        d->setFont(dm.standardFont);
        const uint8_t stdH  = dm.fontHeight(dm.standardFont);
        const uint8_t stdFW = dm.fontWidth(dm.standardFont);

        // ── Title + separator ─────────────────────────────────────────────────
        dm.drawTitleBar(title);
        dm.drawSeparator();

        // ── Page counter in title bar (top-right) ─────────────────────────────
        if (counterTotal > 1) {
            d->setFont(dm.titleFont);
            char pageBuf[24];  // large enough for INT_MAX/INT_MAX
            snprintf(pageBuf, sizeof(pageBuf), "%d/%d", counterCurrent, counterTotal);
            int16_t pgW = (int16_t)d->getStrWidth(pageBuf);
            int16_t pgX = ox + W - pgW - 1;
            int16_t pgY = oy + (int16_t)dm.titleHeight - 2;
            d->setCursor(pgX, pgY);
            d->print(pageBuf);
        }

        d->setFont(dm.standardFont);
        const int16_t contentTop = oy + (int16_t)dm.menuStartY;

        // ── Prepare formatted strings ─────────────────────────────────────────
        char valBuf[16], minBuf[12], maxBuf[12], overBuf[24];
        b->formatValue(valBuf, sizeof(valBuf));
        b->formatMin(minBuf, sizeof(minBuf));
        b->formatMax(maxBuf, sizeof(maxBuf));
        b->formatOverValue(overBuf, sizeof(overBuf));

        // Build the display reading: "value unit" or "value/max unit"
        char readingBuf[28];
        if (b->useOver) {
            if (b->unit && b->unit[0])
                snprintf(readingBuf, sizeof(readingBuf), "%s %s", overBuf, b->unit);
            else
                snprintf(readingBuf, sizeof(readingBuf), "%s", overBuf);
        } else {
            if (b->unit && b->unit[0])
                snprintf(readingBuf, sizeof(readingBuf), "%s%s", valBuf, b->unit);
            else
                snprintf(readingBuf, sizeof(readingBuf), "%s", valBuf);
        }

        const float frac = b->fraction();

        if (b->barType == BAR_COMPACT) {
            // ── COMPACT: two rows ─────────────────────────────────────────────
            // Row 1: item name (left) + value (right)
            int16_t row1Y = contentTop;

            // Value right-aligned
            char summBuf[24];
            b->formatSummary(summBuf, sizeof(summBuf));
            int16_t sumW = (int16_t)d->getStrWidth(summBuf);

            // Auto-scroll item name if it would overlap the value
            {
                int16_t nameAvailW = W - sumW - 3;  // gap between name and value
                int16_t namePixW = (int16_t)d->getStrWidth(itemName);
                if (namePixW <= nameAvailW || nameAvailW <= 0) {
                    d->setCursor(ox + 1, row1Y);
                    d->print(itemName);
                } else {
                    uint8_t nameLen = (uint8_t)strlen(itemName);
                    uint8_t fitC = (nameAvailW > 0 && stdFW > 0) ? (uint8_t)(nameAvailW / stdFW) : 1;
                    if (fitC < 1) fitC = 1;
                    if (fitC > nameLen) fitC = nameLen;
                    unsigned long now = millis();
                    int sr = (int)nameLen - (int)fitC + 1;
                    if (sr < 1) sr = 1;
                    int sp = (int)((now / 400) % (sr * 2));
                    if (sp >= sr) sp = sr * 2 - sp - 1;
                    if (sp < 0) sp = 0;
                    d->setCursor(ox + 1, row1Y);
                    for (uint8_t i = 0; i < fitC && (sp + i) < nameLen; i++)
                        d->print(itemName[sp + i]);
                }
            }

            d->setCursor(ox + W - sumW - 1, row1Y);
            d->print(summBuf);

            // Row 2: bar graph (with optional min/max labels)
            int16_t row2Y = row1Y + stdH + dm.menuItemPadding;
            if (row2Y <= (int16_t)stdH + oy) row2Y = row1Y + stdH + 1;

            drawBarRow(d, ox, oy, W, row2Y, stdH, stdFW, frac,
                       b->showMinMax, minBuf, maxBuf);

        } else {
            // ── DEFAULT: full-screen layout ───────────────────────────────────
            int16_t contentH = (oy + H) - contentTop;

            // Subtitle (item name) with auto-scroll if too wide
            int16_t subtitleY = contentTop;
            {
                int16_t namePixW = (int16_t)d->getStrWidth(itemName);
                int16_t availSubW = W - 2;
                if (namePixW <= availSubW) {
                    d->setCursor(ox + 1, subtitleY);
                    d->print(itemName);
                } else {
                    uint8_t nameLen = (uint8_t)strlen(itemName);
                    uint8_t fitC = (availSubW > 0 && stdFW > 0) ? (uint8_t)(availSubW / stdFW) : 1;
                    if (fitC < 1) fitC = 1;
                    if (fitC > nameLen) fitC = nameLen;
                    unsigned long now = millis();
                    int sr = (int)nameLen - (int)fitC + 1;
                    if (sr < 1) sr = 1;
                    int sp = (int)((now / 400) % (sr * 2));
                    if (sp >= sr) sp = sr * 2 - sp - 1;
                    if (sp < 0) sp = 0;
                    d->setCursor(ox + 1, subtitleY);
                    for (uint8_t i = 0; i < fitC && (sp + i) < nameLen; i++)
                        d->print(itemName[sp + i]);
                }
            }

            // Large centred value
            const uint8_t* usedFont = dm.valueFont;
            uint8_t valFH = dm.fontHeight(dm.valueFont);
            if (valFH > (uint8_t)(contentH * 0.40f)) {
                usedFont = dm.standardFont;
                valFH = stdH;
            }
            d->setFont(usedFont);
            int16_t readW = (int16_t)d->getStrWidth(readingBuf);
            int16_t valueY = contentTop + (int16_t)(contentH * 0.42f);
            int16_t valueX = ox + (W - readW) / 2;
            d->setCursor(valueX, valueY);
            d->print(readingBuf);

            // Bar position
            d->setFont(dm.standardFont);
            int16_t barRowY = contentTop + (int16_t)(contentH * 0.62f);

            // Min/max label row below bar
            drawBarRow(d, ox, oy, W, barRowY, stdH, stdFW, frac,
                       b->showMinMax, minBuf, maxBuf);
        }

        d->sendBuffer();
    }

#endif // MENU_ENABLE_BAR

#if MENU_ENABLE_GAUGE

    //  Layout (full content area):
    //  ┌──────────────────────────────┐
    //  │  Settings                    │  ← title bar
    //  │ ──────────────────────────── │  ← separator
    //  │  Room Temp                   │  ← subtitle (item name)
    //  │  ╭────────────────────╮      │
    //  │  │  ╔══════╗          │      │  ← thick fill arc (FILL style)
    //  │  │  ║ 75C  ║          │      │  ← value+unit centred inside arc
    //  │  │  ╚══════╝          │      │
    //  │  ╰────────────────────╯      │
    //  │  0                    120    │  ← min bottom-left, max bottom-right
    //  └──────────────────────────────┘
    //
    // The arc fills the FULL content width and height (minus a small margin).
    // Value+unit is drawn with valueFont, centred inside the arc circle.
    // Min/max labels are placed at the bottom corners of the content zone.
    //
    void drawGaugeScreen(DisplayGaugeBase* g, const char* title    = "",
                                              const char* itemName = "",
                                              int counterCurrent = 0,
                                              int counterTotal   = 0) {
        U8G2* d = dm.display;
        d->clearBuffer();

        const int16_t ox = dm.ox();
        const int16_t oy = dm.oy();
        const int16_t W  = (int16_t)dm.screenWidth;
        const int16_t H  = (int16_t)dm.screenHeight;
        const float   PI_RAD = 3.14159265f / 180.0f;

        d->setFont(dm.standardFont);
        const uint8_t stdH = dm.fontHeight(dm.standardFont);
        d->setFont(dm.valueFont);
        const uint8_t valH = dm.fontHeight(dm.valueFont);

        // ── Title + separator ─────────────────────────────────────────────────
        const int16_t afterTitle = oy + (int16_t)dm.menuStartY;
        const int16_t totalContentH = (oy + H) - afterTitle;

        const int16_t subtitleRowH = (int16_t)stdH + 2;
        const bool    showSubtitle = (totalContentH - subtitleRowH >= 16);

        if (showSubtitle) dm.drawTitleBar(title);
        else              dm.drawTitleBar((itemName && itemName[0]) ? itemName : title);
        dm.drawSeparator();

        // ── Page counter in title bar (top-right) ─────────────────────────────
        if (counterTotal > 1) {
            d->setFont(dm.titleFont);
            char pageBuf[24];  // large enough for INT_MAX/INT_MAX
            snprintf(pageBuf, sizeof(pageBuf), "%d/%d", counterCurrent, counterTotal);
            int16_t pgW = (int16_t)d->getStrWidth(pageBuf);
            int16_t pgX = ox + W - pgW - 1;
            int16_t pgY = oy + (int16_t)dm.titleHeight - 2;
            d->setCursor(pgX, pgY);
            d->print(pageBuf);
        }

        // ── Subtitle (item name) with auto-scroll if too wide ────────────────
        int16_t contentTop = afterTitle;
        if (showSubtitle) {
            d->setFont(dm.standardFont);
            if (itemName && itemName[0]) {
                int16_t nameW = (int16_t)d->getStrWidth(itemName);
                int16_t availW = W - 2;
                if (nameW <= availW) {
                    d->setCursor(ox + 1, afterTitle);
                    d->print(itemName);
                } else {
                    // Auto-scroll (ping-pong) subtitle
                    uint8_t nameLen = (uint8_t)strlen(itemName);
                    uint8_t fitChars = 0;
                    uint8_t stdFW = dm.fontWidth(dm.standardFont);
                    fitChars = (availW > 0 && stdFW > 0) ? (uint8_t)(availW / stdFW) : 1;
                    if (fitChars < 1) fitChars = 1;
                    if (fitChars > nameLen) fitChars = nameLen;
                    unsigned long now = millis();
                    int scrollRange = (int)nameLen - (int)fitChars + 1;
                    if (scrollRange < 1) scrollRange = 1;
                    int scrollPos = (int)((now / 400) % (scrollRange * 2));
                    if (scrollPos >= scrollRange) scrollPos = scrollRange * 2 - scrollPos - 1;
                    if (scrollPos < 0) scrollPos = 0;
                    d->setCursor(ox + 1, afterTitle);
                    for (uint8_t i = 0; i < fitChars && (scrollPos + i) < nameLen; i++)
                        d->print(itemName[scrollPos + i]);
                }
            }
            contentTop = afterTitle + 1;  // tighter gap — separator already provides spacing
        }

        // ── Prepare label strings early (needed to reserve space) ───────────
        char valBuf[16], readingBuf[24];
        char minBuf[12], maxBuf[12];
        g->formatValue(valBuf, sizeof(valBuf));
        g->formatMin(minBuf, sizeof(minBuf));
        g->formatMax(maxBuf, sizeof(maxBuf));
        if (g->unit && g->unit[0])
            snprintf(readingBuf, sizeof(readingBuf), "%s%s", valBuf, g->unit);
        else
            snprintf(readingBuf, sizeof(readingBuf), "%s", valBuf);

        d->setFont(dm.standardFont);

        // ── Content zone ──────────────────────────────────────────────────────
        // Min/max labels are placed BELOW the gauge (not left/right) so they
        // never shrink the gauge width — critical for small screens.
        const int16_t labelRowH  = g->showMinMax ? (int16_t)stdH + 2 : 0;
        const int16_t gaugeZoneH = (oy + H) - contentTop - labelRowH;
        const int16_t gaugeZoneW = W;
        const int16_t zoneLeft   = ox;

        // ── Arc geometry ──────────────────────────────────────────────────────
        const float startDeg = g->startAngle();
        const float endDeg   = g->endAngle();

        // Sample the arc bounding box to fit radius tightly
        float arcMinSin =  1.0f, arcMaxSin = -1.0f;
        float arcMinCos =  1.0f, arcMaxCos = -1.0f;
        auto sampleA = [&](float a) {
            float s = sinf(a * PI_RAD), c = cosf(a * PI_RAD);
            if (s < arcMinSin) arcMinSin = s;  if (s > arcMaxSin) arcMaxSin = s;
            if (c < arcMinCos) arcMinCos = c;  if (c > arcMaxCos) arcMaxCos = c;
        };
        sampleA(startDeg); sampleA(endDeg);
        for (float card = 0.0f; card <= endDeg; card += 90.0f)
            if (card >= startDeg) sampleA(card);
        if (endDeg > 360.0f) sampleA(360.0f);

        float arcHspan = arcMaxSin - arcMinSin; if (arcHspan < 0.01f) arcHspan = 0.01f;
        float arcWspan = arcMaxCos - arcMinCos; if (arcWspan < 0.01f) arcWspan = 0.01f;

        const int16_t gMargin = 2;
        float rByH = (float)(gaugeZoneH - 2 * gMargin) / arcHspan;
        float rByW = (float)(gaugeZoneW - 2 * gMargin) / arcWspan;
        float rF   = rByH < rByW ? rByH : rByW;
        if (rF < 4.0f) rF = 4.0f;
        const int16_t outerR = (int16_t)rF;

        // Arc thickness: scales with radius, min 3px
        const int16_t arcThick = outerR > 20 ? 6 : (outerR > 12 ? 5 : (outerR > 8 ? 4 : 3));
        const int16_t innerR   = outerR - arcThick;

        // Centre: horizontally centred in the (margin-adjusted) gauge zone.
        // Vertically: start from gaugeZone midpoint, apply arc-asymmetry correction
        // so the arc sits centred within the zone, then clamp so the arc itself
        // (outerR px above/below centre) stays inside contentTop..screenBottom.
        const int16_t gaugeCX = zoneLeft + gaugeZoneW / 2;
        int16_t gaugeCY = contentTop + gaugeZoneH / 2
                        + (int16_t)((-arcMinSin - arcMaxSin) * 0.5f * (float)outerR);
        // Clamp: top of arc must be >= contentTop+1, bottom <= screen bottom-1
        const int16_t arcTop    = (int16_t)(gaugeCY + arcMinSin * (float)outerR);
        const int16_t arcBottom = (int16_t)(gaugeCY + arcMaxSin * (float)outerR);
        if (arcTop < contentTop + 1)
            gaugeCY += (contentTop + 1) - arcTop;
        if (arcBottom > oy + H - 1)
            gaugeCY -= arcBottom - (oy + H - 1);

        // ── Draw fill arc ─────────────────────────────────────────────────────
        // Two-pass approach for clear filled vs unfilled visual separation:
        //
        // Pass 1 — BACKGROUND: draw the full arc as a thin outline (outerR only),
        //          every 4 degrees, so it looks like a dotted/dashed track.
        // Pass 2 — FILL: draw the filled portion as a solid thick arc (innerR→outerR),
        //          every degree, from startDeg up to valueAng.
        //
        // This gives a strong visual contrast regardless of radius size.
        const float valueAng = g->valueAngle();

        // Pass 1: dotted background track (full arc extent)
        for (float a = startDeg; a <= endDeg + 0.5f; a += 4.0f) {
            float rad  = a * PI_RAD;
            float cosA = cosf(rad), sinA = sinf(rad);
            // Draw just the outer ring pixel as the track dot
            d->drawPixel(gaugeCX + (int16_t)(cosA * outerR + 0.5f),
                         gaugeCY + (int16_t)(sinA * outerR + 0.5f));
            // And inner ring pixel so it's visible on small screens
            d->drawPixel(gaugeCX + (int16_t)(cosA * innerR + 0.5f),
                         gaugeCY + (int16_t)(sinA * innerR + 0.5f));
        }

        // Pass 2: solid filled arc from start to current value
        for (float a = startDeg; a <= valueAng + 0.5f; a += 1.0f) {
            float rad  = a * PI_RAD;
            float cosA = cosf(rad), sinA = sinf(rad);
            for (int16_t r = innerR; r <= outerR; r++)
                d->drawPixel(gaugeCX + (int16_t)(cosA * r + 0.5f),
                             gaugeCY + (int16_t)(sinA * r + 0.5f));
        }

        // Endpoint tick marks (short inward lines at arc start and end)
        {
            int16_t tLen = outerR > 12 ? 4 : 3;
            auto drawTick = [&](float angleDeg) {
                float c = cosf(angleDeg * PI_RAD), s = sinf(angleDeg * PI_RAD);
                d->drawLine(gaugeCX + (int16_t)(c * outerR + 0.5f),
                            gaugeCY + (int16_t)(s * outerR + 0.5f),
                            gaugeCX + (int16_t)(c * (outerR - tLen) + 0.5f),
                            gaugeCY + (int16_t)(s * (outerR - tLen) + 0.5f));
            };
            drawTick(startDeg);
            drawTick(endDeg);
        }

        // ── Value label centred inside the arc ────────────────────────────────
        // (valBuf / readingBuf / minBuf / maxBuf already prepared above)

        // Available diameter inside the arc for the value text
        // Use 90% of inner diameter so text has a small margin from the arc
        const int16_t availW = (int16_t)(innerR * 2 - 4);
        const int16_t availH = (int16_t)(innerR - 2);   // lower half only (arc opens down)

        // Try valueFont first, fall back to standardFont if string too wide
        d->setFont(dm.valueFont);
        int16_t readW = (int16_t)d->getStrWidth(readingBuf);
        bool    useBig = (readW <= availW) && ((int16_t)valH <= availH);
        if (!useBig) {
            d->setFont(dm.standardFont);
            readW = (int16_t)d->getStrWidth(readingBuf);
        }
        uint8_t readH = useBig ? valH : stdH;

        // Centre text at arc centre (gaugeCY is arc geometric centre).
        // textY is the U8G2 baseline (bottom of glyphs).
        // Clamp so the full glyph box stays within the content zone.
        int16_t textX = gaugeCX - readW / 2;
        int16_t textY = gaugeCY + (int16_t)(readH / 2);
        // Clamp baseline: text top = textY - readH must be >= contentTop
        if (textY - (int16_t)readH < contentTop)
            textY = contentTop + (int16_t)readH;
        // Clamp baseline: must not exceed screen bottom
        if (textY > oy + H)
            textY = oy + H;
        if (textX < ox) textX = ox;
        if (textX + readW > ox + W) textX = ox + W - readW;
        d->setCursor(textX, textY);
        d->print(readingBuf);

        // ── Min / max labels ─────────────────────────────────────────────────
        // Placed BELOW the gauge as a left/right pair at screen bottom.
        // This avoids stealing horizontal space from the gauge on small screens.
        if (g->showMinMax) {
            d->setFont(dm.standardFont);
            const int16_t labelBaseY = oy + H;   // bottom of screen

            // Min label: left-aligned at bottom
            d->setCursor(ox, labelBaseY);
            d->print(minBuf);

            // Max label: right-aligned at bottom
            int16_t lw = (int16_t)d->getStrWidth(maxBuf);
            d->setCursor(ox + W - lw, labelBaseY);
            d->print(maxBuf);
        }

        d->sendBuffer();
    }


#endif // MENU_ENABLE_GAUGE

    // ── Error screen ─────────────────────────────────────────────────────────
    void drawError(int code, const char* message) {
        U8G2* d = dm.display;
        d->clearBuffer();
        d->setFont(dm.titleFont);
        uint8_t fh = dm.fontHeight(dm.titleFont);
        int16_t y = dm.oy() + fh + 2;
        d->setCursor(dm.ox(), y);
        d->print("ERROR #");
        d->print(code);
        y += fh + 2;
        if (y < dm.oy() + (int16_t)dm.screenHeight) {
            d->setCursor(dm.ox(), y);
            d->print(message);
        }
        y += fh + 2;
        if (y < dm.oy() + (int16_t)dm.screenHeight) {
            d->setFont(dm.standardFont);
            d->setCursor(dm.ox(), y);
            d->print("Press btn");
        }
        d->sendBuffer();
    }

private:
    DisplayManager& dm;

    // Draws a single inline field with the current selection style highlight.
    // Used by date/time editors for per-field highlighting.
    void drawFieldHighlight(U8G2* d, int16_t fieldX, int16_t yPos,
                            uint8_t fh, uint8_t arrowPfx, const char* buf) {
        switch (dm.selectionStyle) {
            case SELECTION_INVERSE: {
                int16_t boxW = (int16_t)d->getStrWidth(buf) + 4;
                int16_t boxX = fieldX - 2;
                int16_t boxY = yPos - (int16_t)fh;
                if (boxY < 0) boxY = 0;
                d->setDrawColor(1);
                d->drawBox(boxX, boxY, boxW, fh + 2);
                d->setFontMode(1);
                d->setDrawColor(0);
                d->setCursor(fieldX, yPos);
                d->print(buf);
                dm.resetDrawColor();
                break;
            }
            case SELECTION_BRACKET:
                d->setCursor(fieldX, yPos);
                d->print("["); d->print(buf); d->print("]");
                break;
            case SELECTION_NONE:
                d->setCursor(fieldX, yPos);
                d->print(buf);
                break;
            case SELECTION_BLINK:
                if (dm.isBlinkVisible()) {
                    d->setCursor(fieldX, yPos);
                    d->print(buf);
                }
                break;
            case SELECTION_ARROW:
            default:
                d->setCursor(fieldX - arrowPfx, yPos);
                d->print("> ");
                d->print(buf);
                break;
        }
    }

    // Draws a single text row using the global selectionStyle.
    void drawRow(U8G2* d, int16_t yPos, uint8_t fh,
                 bool isSel, const char* text) {
        switch (dm.selectionStyle) {
            case SELECTION_INVERSE:
                if (isSel) dm.drawSelectionBox(yPos, fh);
                d->setCursor(dm.ox() + 2, yPos);
                d->print(text);
                if (isSel) dm.resetDrawColor();
                break;
            case SELECTION_BRACKET:
                d->setCursor(dm.ox(), yPos);
                if (isSel) {
                    d->print("["); d->print(text); d->print("]");
                } else {
                    d->print(text);
                }
                break;
            case SELECTION_NONE:
                d->setCursor(dm.ox(), yPos);
                d->print(text);
                break;
            case SELECTION_BLINK:
                if (!isSel || dm.isBlinkVisible()) {
                    d->setCursor(dm.ox(), yPos);
                    d->print(text);
                }
                // When isSel && !blinkVisible, nothing is drawn (item disappears)
                break;
            case SELECTION_ARROW:
            default: {
                uint8_t arrowIndent = dm.arrowIndentWidth(dm.standardFont);
                if (isSel) { d->setCursor(dm.ox(),                yPos); d->print("> "); }
                else        { d->setCursor(dm.ox() + arrowIndent, yPos); }
                d->print(text);
                break;
            }
        }
    }

    static void formatFloat(char* buf, size_t sz, float v, int decimals) {
        if (decimals > 0) {
            char fmt[16];   // large enough for "%.[int]f" with any int value
            snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
            snprintf(buf, sz, fmt, v);
        } else {
            snprintf(buf, sz, "%.0f", v);
        }
    }

    // Draws a vertical list of action rows. If they don't all fit between
    // startY and screenBottom, only show the selected one (visibleItems=1 style).
    void drawActionRows(U8G2* d, int16_t startY, uint8_t fh, uint8_t rowH,
                        const char* const* labels, const bool* selected,
                        uint8_t count) {
        int16_t screenBottom = dm.oy() + (int16_t)dm.screenHeight;
        int16_t availH = screenBottom - startY;
        uint8_t maxVis = (availH > 0 && rowH > 0) ? (uint8_t)(availH / rowH) : 1;
        if (maxVis < 1) maxVis = 1;

        if (maxVis >= count) {
            // All fit — draw them all
            for (uint8_t i = 0; i < count; i++)
                drawRow(d, startY + rowH * i, fh, selected[i], labels[i]);
        } else {
            // Not all fit — scroll so selected is visible
            uint8_t selIdx = 0;
            for (uint8_t i = 0; i < count; i++)
                if (selected[i]) { selIdx = i; break; }
            uint8_t scrollOfs = 0;
            if (selIdx >= maxVis) scrollOfs = selIdx - maxVis + 1;
            if (scrollOfs + maxVis > count) scrollOfs = count - maxVis;
            for (uint8_t v = 0; v < maxVis && (scrollOfs + v) < count; v++) {
                uint8_t idx = scrollOfs + v;
                drawRow(d, startY + rowH * v, fh, selected[idx], labels[idx]);
            }
        }
    }

    // Draws a horizontal bar graph row with optional min/max labels.
    // barY is the text baseline for labels; the bar rectangle is drawn
    // vertically centred relative to the font height.
    //
    //   showMinMax=true:   -20 ┌████████──┐ 50
    //   showMinMax=false:  ┌████████████────────────┐    (full width)
    //
    void drawBarRow(U8G2* d, int16_t ox, int16_t oy, int16_t W,
                    int16_t barY, uint8_t stdH, uint8_t stdFW,
                    float frac,
                    bool showMinMax, const char* minBuf, const char* maxBuf) {

        int16_t barLeft, barRight;

        if (showMinMax) {
            // Measure label widths
            int16_t minW = (int16_t)d->getStrWidth(minBuf);
            int16_t maxW = (int16_t)d->getStrWidth(maxBuf);
            int16_t gap  = stdFW / 2;   // small gap between label and bar
            if (gap < 1) gap = 1;

            barLeft  = ox + minW + gap;
            barRight = ox + W - maxW - gap;

            // Draw min label (left)
            d->setCursor(ox, barY);
            d->print(minBuf);

            // Draw max label (right-aligned)
            d->setCursor(ox + W - maxW, barY);
            d->print(maxBuf);
        } else {
            // Full width bar with small margin
            barLeft  = ox + 1;
            barRight = ox + W - 2;
        }

        int16_t barW = barRight - barLeft;
        if (barW < 4) barW = 4;

        // Bar thickness: scale with font, min 4px, max 10px
        int16_t barThick = stdH - 2;
        if (barThick < 4) barThick = 4;
        if (barThick > 10) barThick = 10;

        // Centre bar vertically on the baseline row
        // baseline is at barY, ascent goes up, so bar top ~ barY - stdH + offset
        // barY is an absolute Y baseline (already includes oy offset)
        int16_t barTopY = barY - stdH + (stdH - barThick) / 2;

        // Outline rectangle (the track)
        d->drawFrame(barLeft, barTopY, barW, barThick);

        // Filled portion (inset 1px for visual clarity)
        int16_t fillW = (int16_t)((float)(barW - 2) * frac);
        if (fillW < 0) fillW = 0;
        if (fillW > barW - 2) fillW = barW - 2;
        if (fillW > 0)
            d->drawBox(barLeft + 1, barTopY + 1, fillW, barThick - 2);
    }
};

#endif // WIDGET_RENDERER_H
