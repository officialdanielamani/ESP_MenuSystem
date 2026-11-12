// MenuConfig.h
// Compile-time configuration for ESP32_MenuSystem: feature switches and
// buffer/array limits. Everything here can be overridden WITHOUT editing the
// library, either
//
//   (a) in the sketch, before the include:
//         #define MENU_ENABLE_STRING 0
//         #define MAX_MENU_DEPTH     6
//         #include <ESP32_MenuSystem.h>
//
//   (b) or as compiler flags (use this when more than one .cpp file includes
//       the library, so every file sees the same values):
//         PlatformIO : build_flags = -DMENU_ENABLE_STRING=0 -DMAX_MENU_DEPTH=6
//         Arduino IDE: put the same -D flags in a build_opt.h next to the sketch
//
// The library is header-only, so the settings take effect for the file that
// includes it. Never mix different settings between files of one program.
#ifndef MENU_CONFIG_H
#define MENU_CONFIG_H

// ═════════════════════════════════════════════════════════════════════════════
// FEATURE SWITCHES  (1 = compiled in, 0 = left out of the firmware)
// ═════════════════════════════════════════════════════════════════════════════
// The menu engine reaches every widget through one render/input dispatcher, so
// the linker cannot drop a widget just because the sketch never creates one.
// Switch off what you do not use to remove its code, strings and fonts.
//
// MENU_MINIMAL turns every optional feature off; then re-enable what you need:
//     #define MENU_MINIMAL
//     #define MENU_ENABLE_DATE 1
#ifdef MENU_MINIMAL
  #define MENU_FEATURE_DEFAULT 0
#else
  #define MENU_FEATURE_DEFAULT 1
#endif

#ifndef MENU_ENABLE_MULTISELECT          // MultiSelectAdjuster + checklist screen
  #define MENU_ENABLE_MULTISELECT MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_DATE                 // DateAdjuster + date editor
  #define MENU_ENABLE_DATE MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_TIME                 // TimeAdjuster + time editor
  #define MENU_ENABLE_TIME MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_STRING               // StringAdjuster + string editor
  #define MENU_ENABLE_STRING MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_GAUGE                // DisplayGaugeInt/Float + arc gauge screen
  #define MENU_ENABLE_GAUGE MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_BAR                  // DisplayBarInt/Float + bar graph screen
  #define MENU_ENABLE_BAR MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_TEXT                 // addMenuText() text / paragraph rows
  #define MENU_ENABLE_TEXT MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_ENCODER              // rotary encoder input (needs ESP32Encoder)
  #define MENU_ENABLE_ENCODER MENU_FEATURE_DEFAULT
#endif
#ifndef MENU_ENABLE_ONE_BUTTON           // single-button input mode
  #define MENU_ENABLE_ONE_BUTTON MENU_FEATURE_DEFAULT
#endif

// ═════════════════════════════════════════════════════════════════════════════
// LIMITS  (these size statically allocated arrays — smaller = less RAM)
// ═════════════════════════════════════════════════════════════════════════════
// RAM used by the menu tree (32-bit MCU, text rows enabled):
//     MAX_MENU_DEPTH * (MAX_MENU_NAME_LENGTH + 16
//                       + MAX_MENU_ITEMS * (MAX_MENU_NAME_LENGTH + 16))
//   defaults 16 / 16 / 24  ->  about 10.9 KB
//   e.g.      6 /  8 / 16  ->  about  1.7 KB

#ifndef MAX_MENU_DEPTH                   // total number of menus (addMenu calls)
  #define MAX_MENU_DEPTH 16
#endif
#ifndef MAX_MENU_ITEMS                   // items per menu
  #define MAX_MENU_ITEMS 16
#endif
#ifndef MAX_MENU_NAME_LENGTH             // item-name / title buffer, INCLUDING '\0'
  #define MAX_MENU_NAME_LENGTH 24
#endif
#ifndef MAX_BOOL_LABEL_LENGTH            // BoolValueAdjuster label buffer, incl. '\0'
  #define MAX_BOOL_LABEL_LENGTH 16
#endif
#ifndef MAX_TEXT_LENGTH                  // max chars drawn per addMenuText() line
  #define MAX_TEXT_LENGTH 64
#endif
#ifndef MAX_ERROR_LENGTH                 // setError() message buffer, incl. '\0'
  #define MAX_ERROR_LENGTH 48
#endif
#ifndef MAX_MULTI_SELECT_OPTIONS         // option slots per MultiSelectAdjuster
  #define MAX_MULTI_SELECT_OPTIONS 16
#endif
#ifndef MAX_STRING_LENGTH                // max chars a StringAdjuster can edit
  #define MAX_STRING_LENGTH 32           // (target buffer = MAX_STRING_LENGTH + 1)
#endif
#ifndef MAX_VISUAL_ROWS                  // rows a menu can expand to (items +
  #define MAX_VISUAL_ROWS 64             // wrapped paragraph lines); stack only
#endif

// ═════════════════════════════════════════════════════════════════════════════
// DEFAULT FONTS
// ═════════════════════════════════════════════════════════════════════════════
// These three fonts are always linked because the menu starts with them.
// If you call setFonts() with your own fonts, point the defaults at the same
// fonts so the unused built-in ones are not stored in flash:
//     #define MENU_DEFAULT_VALUE_FONT u8g2_font_6x12_tr
#ifndef MENU_DEFAULT_STANDARD_FONT       // menu rows
  #define MENU_DEFAULT_STANDARD_FONT u8g2_font_5x8_tr
#endif
#ifndef MENU_DEFAULT_TITLE_FONT          // title bar
  #define MENU_DEFAULT_TITLE_FONT u8g2_font_6x12_tr
#endif
#ifndef MENU_DEFAULT_VALUE_FONT          // large value in editors / gauges
  #define MENU_DEFAULT_VALUE_FONT u8g2_font_10x20_tr
#endif

// ── Sanity checks ────────────────────────────────────────────────────────────
#if MAX_MENU_DEPTH < 1 || MAX_MENU_DEPTH > 127
  #error "MAX_MENU_DEPTH must be 1..127 (menu ids are stored as int8_t)"
#endif
#if MAX_MENU_ITEMS < 1 || MAX_MENU_ITEMS > 255
  #error "MAX_MENU_ITEMS must be 1..255"
#endif
#if MAX_MENU_NAME_LENGTH < 2 || MAX_BOOL_LABEL_LENGTH < 2 || MAX_ERROR_LENGTH < 2
  #error "Name / label / error buffers need at least 2 bytes"
#endif
#if MAX_TEXT_LENGTH < 1 || MAX_TEXT_LENGTH > 254
  #error "MAX_TEXT_LENGTH must be 1..254"
#endif
#if MAX_MULTI_SELECT_OPTIONS < 1 || MAX_MULTI_SELECT_OPTIONS > 254
  #error "MAX_MULTI_SELECT_OPTIONS must be 1..254"
#endif
#if MAX_STRING_LENGTH < 1 || MAX_STRING_LENGTH > 254
  #error "MAX_STRING_LENGTH must be 1..254"
#endif
#if MAX_VISUAL_ROWS < MAX_MENU_ITEMS
  #error "MAX_VISUAL_ROWS must be >= MAX_MENU_ITEMS"
#endif

#endif // MENU_CONFIG_H
