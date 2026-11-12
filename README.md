# ESP32 MenuSystem

A hierarchical menu system for ESP32 + U8g2 displays with support for 1-3 button, and  rotary encoder input modes.

**Version 0.1.2b** · Requires [U8g2](https://github.com/olikraus/u8g2)

Disclamer: Majority of the library and document is re-written by Cloude AI

---

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Constructors](#constructors)
- [Menu Building API](#menu-building-api)
- [Dynamic Menu Behavior](#dynamic-menu-behavior)
- [Value Adjusters](#value-adjusters)
- [String Input](#string-input)
- [Date & Time Editors](#date--time-editors)
- [Editor Actions (Redo / Edit / Save / Back)](#editor-actions-redo--edit--save--back)
- [Gauges & Bar Graphs](#gauges--bar-graphs)
- [Screen Info Overlay](#screen-info-overlay-addscreeninfo)
- [Display & Layout Configuration](#display--layout-configuration)
- [Navigation & State](#navigation--state)
- [Input Configuration](#input-configuration)
- [Reducing Firmware Size](#reducing-firmware-size)
- [Compile-Time Limits](#compile-time-limits)
- [Quick Reference](#quick-reference)

---

## Installation

Copy the `src` folder into your Arduino `libraries/` directory (rename it to `ESP32_MenuSystem`). Install **U8g2** separately.

Rotary-encoder input also needs the **ESP32Encoder** library. If you only use buttons, add `#define MENU_ENABLE_ENCODER 0` before the include and you do not need to install it.

```cpp
#include <ESP32_MenuSystem.h>
```

---

## Quick Start

```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
MenuSystem menu(&u8g2, 25, 26, 27);

void onApply() { /* callback */ }

void setup() {
    u8g2.begin();
    int mainMenu = menu.addMenu("Main");
    int settings = menu.addMenu("Settings");
    menu.addMenuItem(mainMenu, "Settings", settings);
    menu.addMenuItemWithFunction(mainMenu, "Apply", onApply);
    menu.begin();
}

void loop() { menu.update(); }
```

---

## Constructors

```cpp
MenuSystem menu(&u8g2, UP_PIN, DOWN_PIN, OK_PIN);              // 3-button
MenuSystem menu(&u8g2, ENC_A, ENC_B, ENC_BTN, true, sens);     // encoder
MenuSystem menu(&u8g2, BTN_PIN, shortPressIsUp);                // 1-button
```

---

## Menu Building API

| Method | Description |
|--------|-------------|
| `addMenu("Title")` | Create menu screen. Returns ID. |
| `addMenuItem(menuId, "Name", nextMenuId)` | Navigation item. |
| `addMenuItemWithFunction(menuId, "Name", fn)` | Callback item. |
| `addValueMenuItem(menuId, "Name", &adj)` | Editable value (int/float/bool). |
| `addStringMenuItem(menuId, "Name", &strAdj)` | String input. |
| `addDateMenuItem(menuId, "Name", &dateAdj)` | Date editor. |
| `addTimeMenuItem(menuId, "Name", &timeAdj)` | Time editor. |
| `addDisplayGaugeInt(menuId, "Name", &g)` | Circular gauge (int). |
| `addDisplayGaugeFloat(menuId, "Name", &g)` | Circular gauge (float). |
| `addDisplayBarInt(menuId, "Name", &b)` | Bar graph (int). |
| `addDisplayBarFloat(menuId, "Name", &b)` | Bar graph (float). |
| `addMultiSelectMenuItem(menuId, "Name", &ms)` | Multi-select checklist. |
| `addMenuText(menuId, &textPtr, paragraph, font, maxChars)` | Non-selectable text row (single scrolling line or word-wrapped paragraph). |
| `addScreenInfo(menuId, callback)` | Draw overlay callback. |
| `renameItem(menuId, itemIndex, "New")` | Change an item label at runtime. |
| `renameMenuTitle(menuId, "New")` | Change a menu title at runtime. |

Each optional widget can be compiled out — its `addXxx()` method then no longer exists. See [Reducing Firmware Size](#reducing-firmware-size).

---

## Dynamic Menu Behavior

**Add at runtime?** Yes — any `addXxx()` works after `setup()` (up to `MAX_MENU_ITEMS` = 16 items per menu).

**Remove at runtime?** No — manage visibility in your logic instead.

**Rename at runtime?** Yes — `renameItem()` / `renameMenuTitle()`. Names are copied into a fixed `char[MAX_MENU_NAME_LENGTH]` buffer (default 24 = 23 visible characters); longer names are cut off. For free-form dynamic text use `addMenuText()` or `addScreenInfo()`.

---

## Value Adjusters

```cpp
IntValueAdjuster adj(&brightness, 5, 0, 100, "%");        // int
FloatValueAdjuster adj(&temp, 0.5, 10.0, 40.0, 1, "C");  // float
BoolValueAdjuster adj(&wifi, "Enabled", "Disabled");       // bool (labels: max 15 chars)

MultiSelectAdjuster ms;
ms.addOption("Monday", true);
ms.addOption("Tuesday", false);
```

---

## String Input

The `StringAdjuster` provides character-by-character string editing with a fully configurable character set (palette), minimum/maximum length enforcement, and no heap allocation.

### Basic Usage

```cpp
char myName[33] = "Hello";
StringAdjuster sa(myName, STRING_PALETTE_ALPHANUM, 1, 16);
menu.addStringMenuItem(settingsMenu, "Name", &sa);
```

**Constructor:** `StringAdjuster(char* value, const char* palette, uint8_t minLen, uint8_t maxLen)`

| Parameter | Description |
|-----------|-------------|
| `value` | Target `char[]` buffer (at least `MAX_STRING_LENGTH + 1` = 33 bytes). |
| `palette` | Allowed characters. Use predefined macros or custom strings. |
| `minLen` | Minimum length. `0` = empty allowed. |
| `maxLen` | Maximum length. Capped to `MAX_STRING_LENGTH` (32). |

### Predefined Palettes

Palettes are `#define` string literals — combine by placing next to each other (compiler merges at compile time):

**Building blocks:**

| Macro | Characters |
|-------|-----------|
| `STRING_PALETTE_UPPER` | `ABCDEFGHIJKLMNOPQRSTUVWXYZ` |
| `STRING_PALETTE_LOWER` | `abcdefghijklmnopqrstuvwxyz` |
| `STRING_PALETTE_DIGITS` | `0123456789` |
| `STRING_PALETTE_HEX_AF` | `ABCDEF` |
| `STRING_PALETTE_SYMBOL` | `!@#$%^&*()-_=+.,?/` |

**Ready-made combos:** `STRING_PALETTE_ALPHA`, `STRING_PALETTE_ALPHANUM`, `STRING_PALETTE_HEX`

```cpp
StringAdjuster sa(buf, STRING_PALETTE_UPPER STRING_PALETTE_DIGITS STRING_PALETTE_SYMBOL, 0, 20);
StringAdjuster phone(buf, "+-()" STRING_PALETTE_DIGITS, 1, 20);
StringAdjuster dna(buf, "ACGT", 1, 8);
```

### Character Input Screen

```
Title
──────────────────────
H e l l [o] _ _ _          ← string (auto-scrolls if wider than screen)
       SPACE                ← current selection (title font, centred)
Pos:5/16 | 1-16            ← position and min-max range
↑↓ scroll                  ← hint
```

Controls appear first in scroll order, then user palette characters:

| Entry | Action |
|-------|--------|
| `OK` | Accept current character, advance to next position. |
| `BACK` | Go back to previous character position. |
| `DEL` | Delete character at current position. |
| `SPACE` | Insert a space character. |
| `DONE` | Finish input, go to action screen. |
| `A`, `B`, ... | User palette characters. |

> `DONE` only appears when `tempLen >= minLength`.

### Action Screen

```
Title
──────────────────────
"Tester"                    ← preview (auto-scrolls if too long)
6/16 chars                  ← length / max
  Redo
  Edit
> Save
  < Back
```

See [Editor Actions](#editor-actions-redo--edit--save--back) for what each row does.

### Examples

```cpp
char pin[33] = "0000";
StringAdjuster pinAdj(pin, STRING_PALETTE_DIGITS, 4, 4);

char ssid[33] = "";
StringAdjuster ssidAdj(ssid, STRING_PALETTE_ALPHANUM "-_.", 1, 32);

char color[33] = "FF8800";
StringAdjuster colorAdj(color, STRING_PALETTE_HEX, 6, 6);

char phone[33] = "+60";
StringAdjuster phoneAdj(phone, "+-()" STRING_PALETTE_DIGITS, 3, 20);
```

---

## Date & Time Editors

```cpp
int day = 21, month = 8, year = 2021;
DateAdjuster da(&day, &month, &year,
                DATE_DAY | DATE_MONTH | DATE_YEAR,
                DATE_ORDER_DMY, DATE_MONTH_SHORT, 2000, 2099);
menu.addDateMenuItem(settingsMenu, "Date", &da);
```

```cpp
int h = 13, m = 32, s = 0;
TimeAdjuster ta(&h, &m, &s,
                TIME_HOUR | TIME_MINUTE | TIME_SECOND, TIME_MODE_24);
ta.setMinuteRange(0, 59, 5);
menu.addTimeMenuItem(settingsMenu, "Time", &ta);
```

After editing fields, the action screen shows **Redo / Edit / Save / Back** — see [Editor Actions](#editor-actions-redo--edit--save--back).

---

## Editor Actions (Redo / Edit / Save / Back)

All editors (String, Date, Time) share the same four action rows. The target variable is **never modified** until **Save**.

| Action | What it does |
|--------|-------------|
| **Redo** | Reset to the value when the editor was opened. Returns to field/char input. |
| **Edit** | Go back to field/char input **keeping your current changes**. |
| **Save** | Write the edited value to the target variable. Exit editor. |
| **Back** | Discard all changes. Exit editor. Target variable unchanged. |

### Example walkthrough

```
Name = "Hello" (stored in char*)

1. User opens editor       → snapshot = "Hello"
2. User types "Tester"     → temp = "Tester", Name still = "Hello"
3. User selects DONE       → action screen shows preview "Tester"

   If Redo:  temp resets to "Hello", back to char input
   If Edit:  temp stays "Tester", back to char input (can change to "World")
   If Save:  Name = "Tester" (or "World" if edited), editor closes
   If Back:  Name stays "Hello", editor closes
```

> **Edit vs Redo:** Edit keeps your work. Redo discards it. Both return to the input screen. Neither touches the stored variable.

---

## Gauges & Bar Graphs

```cpp
DisplayGaugeInt gauge(&rpm, 0, 8000, "rpm", 270, true);
menu.addDisplayGaugeInt(dashMenu, "RPM", &gauge);

DisplayBarInt bar(&load, 0, 100, "%", BAR_DEFAULT, true, false);
menu.addDisplayBarInt(dashMenu, "CPU", &bar);
```

| Type | Layout |
|------|--------|
| `BAR_DEFAULT` | Full-screen with large value + bar |
| `BAR_COMPACT` | Two-row inline |

---

## Screen Info Overlay (`addScreenInfo`)

```cpp
void drawStatusIcons() {
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(112, 10, 0x0050);
}
menu.addScreenInfo(mainMenu, drawStatusIcons);
```

Callback runs every frame after menu is drawn, before `sendBuffer()`. Variables update automatically.

---

## Display & Layout Configuration

### Font Presets
```cpp
menu.setFontPreset(FONT_PRESET_NORMAL);  // SMALL, NORMAL, LARGE
```

### Selection Styles

| Style | Enum | Appearance |
|-------|------|------------|
| Arrow | `SELECTION_ARROW` | `> ` prefix (default) |
| Inverse | `SELECTION_INVERSE` | White-on-black box |
| Bracket | `SELECTION_BRACKET` | `[brackets]` |
| None | `SELECTION_NONE` | No cursor — max text space |
| Blink | `SELECTION_BLINK` | Selected item blinks |

```cpp
menu.setSelectionStyle(SELECTION_BLINK);
menu.setBlinkInterval(300);  // default 400ms half-cycle
```

### Per-Menu Layout
```cpp
menu.setMenuLayout(menuId, maxChars, visibleItems, anchor, showCounter);
```

### Other Settings

| Method | Effect |
|--------|--------|
| `setShowSeparator(bool)` | Title separator line |
| `setShowScrollBar(bool)` | Right-edge scroll bar |
| `setMenuItemPadding(px)` | Fixed inter-row padding |
| `setDisplayOffset(x, y)` | Shift all drawing |
| `setScreenSize(w, h)` | Override screen size |
| `setBlinkInterval(ms)` | Blink speed (default 400) |

---

## Navigation & State

| Method | Description |
|--------|-------------|
| `getCursorPosition()` | Current cursor index (0-based) |
| `getCurrentMenuId()` | ID of displayed menu |
| `setCurrentMenu(menuId)` | Jump to menu |
| `goBack()` | Return to root menu |
| `setError(code, msg)` | Show error screen |

---

## Input Configuration

```cpp
menu.configureButtonTriggers(TRIGGER_LOW, TRIGGER_LOW, TRIGGER_HIGH);
menu.setLongPressThreshold(2000);  // single-button (default 3000ms)
```

---

## Reducing Firmware Size

The menu engine draws and handles every widget through one dispatcher, so the linker cannot remove a widget just because your sketch never creates one. By default **everything is compiled in**. Switch off what you do not use and its code, strings and fonts are left out of the firmware.

### Feature switches

Define a switch as `0` **before** including the library:

```cpp
#define MENU_ENABLE_STRING 0     // no StringAdjuster / string editor
#define MENU_ENABLE_GAUGE  0     // no gauges
#include <ESP32_MenuSystem.h>
```

Or start from nothing and enable only what you need:

```cpp
#define MENU_MINIMAL             // all optional features off
#define MENU_ENABLE_TIME 1       // ...except the time editor
#include <ESP32_MenuSystem.h>
```

| Switch | Removes when `0` | Approx. flash saved* |
|--------|------------------|---------------------|
| `MENU_ENABLE_STRING` | `StringAdjuster`, `addStringMenuItem()` | 6.3 KB |
| `MENU_ENABLE_DATE` | `DateAdjuster`, `addDateMenuItem()` | 4.0 KB |
| `MENU_ENABLE_GAUGE` | `DisplayGaugeInt/Float`, `addDisplayGaugeXxx()` | 3.7 KB |
| `MENU_ENABLE_TIME` | `TimeAdjuster`, `addTimeMenuItem()` | 3.3 KB |
| `MENU_ENABLE_BAR` | `DisplayBarInt/Float`, `addDisplayBarXxx()` | 2.8 KB |
| `MENU_ENABLE_MULTISELECT` | `MultiSelectAdjuster`, `addMultiSelectMenuItem()` | 2.2 KB |
| `MENU_ENABLE_TEXT` | `addMenuText()` (and the `4x6` font it links) | 1.7 KB + font, and 4 bytes RAM per item slot |
| `MENU_ENABLE_ONE_BUTTON` | 1-button constructor and its on-screen hints | 0.4 KB |
| `MENU_ENABLE_ENCODER` | Encoder constructor **and the ESP32Encoder dependency** | — |
| `MENU_MINIMAL` | All of the above | about 28 KB (menu engine drops to roughly 40 % of full size) |

Menus, callbacks, int/float/bool editors, `addScreenInfo()` and 3-button input are always available.

\* Measured with a desktop GCC build (`-Os`) of a two-item menu, not on an ESP32. Use the numbers to compare features with each other; the real savings on your board will differ.

Using a disabled feature is a compile error (for example `'DateAdjuster' was not declared`) — set its switch back to `1`.

### Fonts

The three default fonts are always linked. If you call `setFonts()` with your own, point the defaults at them too so the built-in ones (the `10x20` value font is the largest) are not stored in flash:

```cpp
#define MENU_DEFAULT_STANDARD_FONT u8g2_font_5x8_tr
#define MENU_DEFAULT_TITLE_FONT    u8g2_font_6x12_tr
#define MENU_DEFAULT_VALUE_FONT    u8g2_font_6x12_tr   // instead of 10x20
#include <ESP32_MenuSystem.h>
```

`setFontPreset()` only links its extra fonts if your sketch calls it.

### Where to put the defines

| Project type | How |
|--------------|-----|
| Single sketch (`.ino`) | `#define` lines above `#include <ESP32_MenuSystem.h>` |
| PlatformIO | `build_flags = -DMENU_MINIMAL -DMENU_ENABLE_TIME=1 -DMAX_MENU_DEPTH=6` |
| Arduino IDE, several `.cpp` files | Same `-D` flags in a `build_opt.h` file next to the sketch |

> The library is header-only. If more than one source file includes it, **every file must see the same settings** — use compiler flags in that case, not per-file `#define`s.

All switches and limits live in `src/MenuConfig.h`.

---

## Compile-Time Limits

Menus are stored in fixed arrays (no heap), so these limits decide how much RAM the library uses. Override them the same way as the feature switches.

| Define | Default | Meaning |
|--------|---------|---------|
| `MAX_MENU_DEPTH` | 16 | Total number of menus (`addMenu()` calls) |
| `MAX_MENU_ITEMS` | 16 | Items per menu |
| `MAX_MENU_NAME_LENGTH` | 24 | Item-name / title buffer, including the terminator (23 visible chars) |
| `MAX_BOOL_LABEL_LENGTH` | 16 | `BoolValueAdjuster` label buffer (15 visible chars) |
| `MAX_STRING_LENGTH` | 32 | Chars a `StringAdjuster` can edit |
| `MAX_MULTI_SELECT_OPTIONS` | 16 | Multi-select choices |
| `MAX_TEXT_LENGTH` | 64 | Chars drawn per `addMenuText()` line |
| `MAX_ERROR_LENGTH` | 48 | `setError()` message buffer (47 visible chars) |
| `MAX_VISUAL_ROWS` | 64 | Rows one menu can expand to (items + wrapped paragraph lines) |

Text longer than its buffer is truncated, never overflowed. Adding beyond `MAX_MENU_DEPTH` / `MAX_MENU_ITEMS` is ignored (`addMenu()` returns `-1`).

### RAM used by the menu tree

```
MAX_MENU_DEPTH × (MAX_MENU_NAME_LENGTH + 16 + MAX_MENU_ITEMS × (MAX_MENU_NAME_LENGTH + 16))
```

| Menus × items × name | RAM (ESP32) |
|----------------------|-------------|
| 16 × 16 × 32 (old defaults) | ≈ 13.1 KB |
| 16 × 16 × 24 (defaults) | ≈ 10.9 KB |
| 8 × 8 × 16 | ≈ 2.3 KB |
| 4 × 6 × 16 | ≈ 0.9 KB |

```cpp
#define MAX_MENU_DEPTH       6
#define MAX_MENU_ITEMS       8
#define MAX_MENU_NAME_LENGTH 16
#include <ESP32_MenuSystem.h>
```

Each adjuster object you create also holds its own buffers: a `StringAdjuster` about `2 × MAX_STRING_LENGTH`, a `BoolValueAdjuster` `2 × MAX_BOOL_LABEL_LENGTH`, a `MultiSelectAdjuster` about `6 × MAX_MULTI_SELECT_OPTIONS` bytes.

---

## Quick Reference

```cpp
MenuSystem menu(&u8g2, UP, DOWN, OK);
int m = menu.addMenu("Title");
menu.begin();

menu.addMenuItem(m, "Name", targetMenuId);
menu.addMenuItemWithFunction(m, "Name", myFunc);
menu.addValueMenuItem(m, "Name", &intAdj);
menu.addStringMenuItem(m, "Name", &strAdj);
menu.addDateMenuItem(m, "Name", &dateAdj);
menu.addTimeMenuItem(m, "Name", &timeAdj);
menu.addDisplayGaugeInt(m, "Name", &gauge);
menu.addDisplayBarInt(m, "Name", &bar);
menu.addMultiSelectMenuItem(m, "Name", &ms);
menu.addScreenInfo(m, myDrawCallback);

menu.setSelectionStyle(SELECTION_INVERSE);
menu.setFontPreset(FONT_PRESET_NORMAL);
menu.setBlinkInterval(400);

menu.update();  // call in loop()
```
