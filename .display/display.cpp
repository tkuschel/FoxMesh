/**
 * @file display.cpp
 * @brief Thread-safe asynchronous SSD1306 OLED display driver implementation
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "display.hpp"
#include <cstring>
#include <cstdio>
#include "driver/i2c_types.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.hpp"

static const char *TAG = "DISPLAY";

// Standard 5x7 ASCII Font Table (covers characters from ASCII 32 ' ' to 126 '~')
// Each character consists of 5 vertical column bytes
// Extended with German Umlaute at the end: Ä (95), Ö (96), Ü (97), ä (98), ö (99), ü (100), ß (101)
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 Space
    {0x00, 0x00, 0x4f, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // 35 #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // 41 )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 48 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // 64 @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // 65 A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // 68 D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // 71 G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // 72 H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // 74 J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // 77 M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // 78 N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // 79 O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // 81 Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // 84 T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // 85 U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // 86 V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 Backslash
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // 102 f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // 103 g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // 106 j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // 107 k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // 108 l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // 113 q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // 116 t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // 117 u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // 118 v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // 121 y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // 122 z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 {
    {0x00, 0x00, 0x77, 0x00, 0x00}, // 124 |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 }
    {0x08, 0x08, 0x2a, 0x1c, 0x08}, // 126 ~
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 127 DEL (Control character)
    // --- UTF-8 0xC2 RANGE (ISO-8859-1 Indizes 128 to 191) ---
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 128 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 129 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 130 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 131 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 132 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 133 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 134 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 135 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 136 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 137 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 138 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 139 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 140 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 141 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 142 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 143 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 144 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 145 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 146 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 147 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 148 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 149 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 150 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 151 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 152 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 153 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 154 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 155 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 156 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 157 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 158 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 159 Padding (Control character)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 160 Non-breaking space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // 161 Inverted exclamation mark (¡)
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // 162 Cent sign (¢)
    {0x10, 0x7e, 0x24, 0x44, 0x28}, // 163 Pound sign (£)
    {0x6c, 0x28, 0x28, 0x28, 0x6c}, // 164 Currency sign (¤)
    {0x08, 0x3e, 0x49, 0x3e, 0x08}, // 165 Yen sign (¥)
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // 166 Broken bar (¦) (Alternativ durchgezogen)
    {0x22, 0x49, 0x49, 0x49, 0x22}, // 167 Section sign / Paragraph (§)
    {0x00, 0x03, 0x00, 0x03, 0x00}, // 168 Diaeresis (¨)
    {0x3e, 0x41, 0x5d, 0x49, 0x3e}, // 169 Copyright sign (©)
    {0x44, 0x28, 0x3e, 0x44, 0x3e}, // 170 Feminine ordinal indicator (ª)
    {0x08, 0x14, 0x22, 0x00, 0x00}, // 171 Left-pointing double-angle quotation mark («)
    {0x04, 0x04, 0x04, 0x04, 0x07}, // 172 Not sign (¬)
    {0x04, 0x04, 0x04, 0x04, 0x04}, // 173 Soft hyphen
    {0x3e, 0x41, 0x5d, 0x55, 0x3e}, // 174 Registered sign (®)
    {0x01, 0x01, 0x01, 0x01, 0x01}, // 175 Macron (¯)
    {0x00, 0x06, 0x09, 0x06, 0x00}, // 176 Degree sign (°)
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // 177 Plus-minus sign (±) (Unterstrich wird dynamisch generiert/Zweit-Byte)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 178 Superscript two (²) -> Oft custom generiert
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 179 Superscript three (³) -> Oft custom generiert
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 180 Acute accent (´)
    {0x3c, 0x0a, 0x0a, 0x0a, 0x02}, // 181 Micro sign (µ)
    {0x06, 0x7f, 0x4b, 0x4b, 0x3a}, // 182 Pilcrow sign / Paragraph sign (¶)
    {0x00, 0x00, 0x0c, 0x0c, 0x00}, // 183 Middle dot (·)
    {0x10, 0x20, 0x00, 0x00, 0x00}, // 184 Cedilla (¸)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 185 Superscript one (¹)
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 186 Masculine ordinal indicator (º)
    {0x00, 0x00, 0x22, 0x14, 0x08}, // 187 Right-pointing double-angle quotation mark (»)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 188 Vulgar fraction one quarter (¼)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 189 Vulgar fraction one half (½)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 190 Vulgar fraction three quarters (¾)
    {0x30, 0x48, 0x4a, 0x40, 0x20}, // 191 Inverted question mark (¿)
    // --- UTF-8 0xC3 RANGE (ISO-8859-1 Indizes 192 to 255) ---
    {0x7c, 0x13, 0x11, 0x10, 0x7c}, // 192 Capital A with grave (À)
    {0x7c, 0x10, 0x11, 0x13, 0x7c}, // 193 Capital A with acute (Á)
    {0x7c, 0x12, 0x15, 0x12, 0x7c}, // 194 Capital A with circumflex (Â)
    {0x7c, 0x12, 0x15, 0x12, 0x7c}, // 195 Capital A with tilde (Ã)
    {0x7c, 0x11, 0x11, 0x11, 0x7c}, // 196 Capital A with umlaut (Ä)
    {0x7c, 0x11, 0x11, 0x11, 0x7c}, // 197 Capital A with ring above (Å)
    {0x7f, 0x49, 0x7f, 0x41, 0x63}, // 198 Capital AE ligature (Æ)
    {0x3e, 0x41, 0x41, 0x22, 0x18}, // 199 Capital C with cedilla (Ç)
    {0x7f, 0x4a, 0x4a, 0x42, 0x42}, // 200 Capital E with grave (È)
    {0x7f, 0x42, 0x4a, 0x4a, 0x42}, // 201 Capital E with acute (É)
    {0x7f, 0x43, 0x4d, 0x43, 0x41}, // 202 Capital E with circumflex (Ê)
    {0x7f, 0x41, 0x41, 0x41, 0x41}, // 203 Capital E with umlaut (Ë)
    {0x00, 0x42, 0x7d, 0x40, 0x00}, // 204 Capital I with grave (Ì)
    {0x00, 0x40, 0x7d, 0x42, 0x00}, // 205 Capital I with acute (Í)
    {0x00, 0x42, 0x7d, 0x42, 0x00}, // 206 Capital I with circumflex (Î)
    {0x00, 0x41, 0x7d, 0x41, 0x00}, // 207 Capital I with umlaut (Ï)
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // 208 Capital Eth (Ð)
    {0x7f, 0x09, 0x05, 0x03, 0x7f}, // 209 Capital N with tilde (Ñ)
    {0x3e, 0x43, 0x41, 0x41, 0x3e}, // 210 Capital O with grave (Ò)
    {0x3e, 0x41, 0x41, 0x43, 0x3e}, // 211 Capital O with acute (Ó)
    {0x3e, 0x42, 0x45, 0x42, 0x3e}, // 212 Capital O with circumflex (Ô)
    {0x3e, 0x42, 0x45, 0x42, 0x3e}, // 213 Capital O with tilde (Õ)
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // 214 Capital O with umlaut (Ö)
    {0x08, 0x14, 0x22, 0x14, 0x08}, // 215 Multiplication sign (×)
    {0x3e, 0x61, 0x51, 0x4b, 0x3e}, // 216 Capital O with stroke (Ø)
    {0x3f, 0x42, 0x40, 0x40, 0x3f}, // 217 Capital U with grave (Ù)
    {0x3f, 0x40, 0x40, 0x42, 0x3f}, // 218 Capital U with acute (Ú)
    {0x3f, 0x42, 0x44, 0x42, 0x3f}, // 219 Capital U with circumflex (Û)
    {0x3f, 0x41, 0x40, 0x41, 0x3f}, // 220 Capital U with umlaut (Ü)
    {0x03, 0x04, 0x78, 0x04, 0x03}, // 221 Capital Y with acute (Ý)
    {0x7f, 0x49, 0x49, 0x36, 0x00}, // 222 Capital Thorn (Þ)
    {0x3e, 0x49, 0x49, 0x49, 0x32}, // 223 Small sharp s / Eszett (ß)
    {0x20, 0x55, 0x55, 0x54, 0x78}, // 224 Small a with grave (à)
    {0x20, 0x54, 0x55, 0x55, 0x78}, // 225 Small a with acute (á)
    {0x20, 0x56, 0x55, 0x56, 0x78}, // 226 Small a with circumflex (â)
    {0x20, 0x56, 0x55, 0x56, 0x78}, // 227 Small a with tilde (ã)
    {0x20, 0x55, 0x54, 0x55, 0x78}, // 228 Small a with umlaut (ä)
    {0x20, 0x55, 0x54, 0x55, 0x78}, // 229 Small a with ring above (å)
    {0x24, 0x54, 0x78, 0x54, 0x5c}, // 230 Small ae ligature (æ)
    {0x38, 0x44, 0x44, 0x28, 0x10}, // 231 Small c with cedilla (ç)
    {0x38, 0x55, 0x55, 0x54, 0x18}, // 232 Small e with grave (è)
    {0x38, 0x54, 0x55, 0x55, 0x18}, // 233 Small e with acute (é)
    {0x38, 0x56, 0x55, 0x56, 0x18}, // 234 Small e with circumflex (ê)
    {0x38, 0x55, 0x54, 0x55, 0x18}, // 235 Small e with umlaut (ë)
    {0x00, 0x45, 0x7c, 0x40, 0x00}, // 236 Small i with grave (ì)
    {0x00, 0x40, 0x7c, 0x45, 0x00}, // 237 Small i with acute (í)
    {0x00, 0x46, 0x7c, 0x46, 0x00}, // 238 Small i with circumflex (î)
    {0x00, 0x45, 0x7c, 0x45, 0x00}, // 239 Small i with umlaut (ï)
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // 240 Small eth (ð)
    {0x7c, 0x05, 0x05, 0x04, 0x78}, // 241 Small n with tilde (ñ)
    {0x38, 0x45, 0x45, 0x44, 0x38}, // 242 Small o with grave (ò)
    {0x38, 0x44, 0x45, 0x45, 0x38}, // 243 Small o with acute (ó)
    {0x38, 0x46, 0x45, 0x46, 0x38}, // 244 Small o with circumflex (ô)
    {0x38, 0x46, 0x45, 0x46, 0x38}, // 245 Small o with tilde (õ)
    {0x38, 0x45, 0x44, 0x45, 0x38}, // 246 Small o with umlaut (ö)
    {0x08, 0x08, 0x2a, 0x08, 0x08}, // 247 Division sign (÷)
    {0x38, 0x64, 0x54, 0x4c, 0x38}, // 248 Small o with stroke (ø)
    {0x3c, 0x41, 0x40, 0x20, 0x7c}, // 249 Small u with grave (ù)
    {0x3c, 0x40, 0x40, 0x21, 0x7c}, // 250 Small u with acute (ú)
    {0x3c, 0x42, 0x40, 0x22, 0x7c}, // 251 Small u with circumflex (û)
    {0x3c, 0x41, 0x40, 0x21, 0x7c}, // 252 Small u with umlaut (ü)
    {0x4c, 0x30, 0x30, 0x30, 0x7c}, // 253 Small y with acute (ý)
    {0x7f, 0x24, 0x24, 0x18, 0x00}, // 254 Small thorn (þ)
    {0x4c, 0x31, 0x30, 0x31, 0x7c}  // 255 Small y with umlaut (ÿ)
};

/**
 * @brief Construct a new Display Controller object and register the device on the I2C bus.
 * @param bus_pointer Pointer to the active, thread-safe hardware I2C master bus instance.
 */
DisplayController::DisplayController(i2c_master_bus_handle_t *i2c_handle) : m_bus(i2c_handle) {
    if (m_bus == nullptr) {
        ESP_LOGE(TAG, "Failed creation: Input I2C bus pointer context is null!");
        return;
    }

    // Configure the specific device attachment parameters tracking your SSD1306 layout
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = static_cast<uint16_t>(Config::OLED_ADDRESS);
    dev_config.scl_speed_hz = Config::I2C_FREQ_HZ;

    // Safely attach the display device context onto OM Tom's infrastructure bus handle
    esp_err_t err = i2c_master_bus_add_device(m_bus->get_bus_handle(), &dev_config, &m_devHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SSD1306 on the I2C Master Bus: %s", esp_err_to_name(err));
        m_devHandle = nullptr;
    }
}

/**
 * @brief Destroy the Display Controller object, freeing up allocated queue and device resources.
 */
DisplayController::~DisplayController() {
    if (m_devHandle != nullptr) {
        i2c_master_bus_rm_device(m_devHandle);
    }
    if (m_msgQueue != nullptr) {
        vQueueDelete(m_msgQueue);
    }
}

/**
 * @brief Internal helper to dispatch low-overhead raw hardware command sequences to the display.
 * @param cmd The raw execution command configuration byte.
 * @return esp_err_t status metric indicating whether the transfer succeeded.
 */
esp_err_t DisplayController::sendCommand(uint8_t cmd) const {
    if (m_devHandle == nullptr) return ESP_ERR_INVALID_STATE;

    // SSD1306 command stream control stream frame format payload byte array
    uint8_t buffer[2] = { 0x00, cmd };
    return i2c_master_transmit(m_devHandle, buffer, sizeof(buffer), 100);
}

/**
 * @brief Triggers the fundamental register setup and executes a ultra-reliable high-speed screen wipe.
 * @return esp_err_t status tracking variable collecting transaction anomalies.
 */
esp_err_t DisplayController::initializeHardware() {
    if (m_devHandle == nullptr) return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;

        // Determine target-specific RAM widths
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    const uint8_t displayWidth = 132; // SH1106 total internal RAM columns
#else
    const uint8_t displayWidth = 128; // SSD1306 total internal RAM columns
#endif

    // Ultra-reliable fundamental minimal initialization sequence configuration steps
    err |= sendCommand(0xAE); // Turn display off
    err |= sendCommand(0xD5); // Set clock divide ratio
    err |= sendCommand(0x80);
    err |= sendCommand(0xA8); // Set multiplex ratio
    err |= sendCommand(0x3F); // 1/64 duty cycle setup
    err |= sendCommand(0xD3); // Set display offset
    err |= sendCommand(0x00); // No offset
    err |= sendCommand(0x40); // Set start line at 0
    err |= sendCommand(0x8D); // Charge pump configuration setup
    err |= sendCommand(0x14); // Enable charge pump power rails
    err |= sendCommand(0x20); // Set memory addressing mode profile
    err |= sendCommand(0x02); // Page addressing mode (highly predictable translation)
    err |= sendCommand(0xA1); // Segment re-map columns configuration matrix
    err |= sendCommand(0xC8); // COM Output scan direction setting execution
    err |= sendCommand(0xDA); // COM pins hardware structural layout profile
    err |= sendCommand(0x12);
    err |= sendCommand(0x81); // Contrast configuration mapping setting
    err |= sendCommand(0x7F);
    err |= sendCommand(0xA4); // Resume display to RAM mapping entries
    err |= sendCommand(0xA6); // Normal display output formatting selection profile

    // High-Speed Screen Clear on startup to guarantee zero RAM noise artifacts
    for (uint8_t page = 0; page < 8; page++) {
        err |= sendCommand(static_cast<uint8_t>(0xB0 + page)); // Set page boundary
        err |= sendCommand(0x00);                              // Reset lower column pointer
        err |= sendCommand(0x10);                              // Reset higher column pointer

        // Allocate high-speed contiguous flash transmission block buffer (1 control byte + 128 data bytes)
        // FIXED: Dynamically allocate clear-buffer based on target RAM size (1 control byte + width)
        // Max size is 133 bytes for SH1106, securely initialised to zeros
        uint8_t clearBuffer[133] = {0};
        clearBuffer[0] = 0x40; // Enforce explicit Data Mode streaming identifier token

        // The remaining 128 bytes are already natively initialized to 0x00 via the aggregate initializer {0}
        if (m_devHandle != nullptr) {
            err |= i2c_master_transmit(m_devHandle, clearBuffer, static_cast<size_t>(displayWidth + 1), 50);
        }
    }

    err |= sendCommand(0xAF); // Wake up physical controller panel (display ON)

    return err;
}

/**
 * @brief Instantiates the static FreeRTOS messaging pipeline and deploys the background consumer thread onto Core 0.
 * @param priority Task priority rank assignment metric handled by the scheduler.
 */
void DisplayController::startTask(uint32_t priority) {
    // Allocate the static thread-safe communication data path
    m_msgQueue = xQueueCreate(8, sizeof(DisplayMessage));
    if (m_msgQueue == nullptr) {
        ESP_LOGE(TAG, "Critical error: Could not allocate memory for display communication queue!");
        return;
    }

    // Deploy onto Core 0 alongside the terminal pipeline to keep Core 1 free for LoRa scheduling
    xTaskCreatePinnedToCore(
        DisplayController::taskTrampoline,
        "OLED_WorkerTask",
        4096,
        this,
        priority,
        nullptr,
        0
    );
}
/**
 * @brief Safe cross-core interface dispatching a text block onto the internal processing pipeline buffer.
 * @param line Selected hardware page index mapping from 0 to 7 (Bit 7 disables Auto-Clear).
 * @param text Pointer to a standard null-terminated sequence of tracking information.
 * @param startX Starting column coordinate index on the horizontal axis (0 to 127).
 */
void DisplayController::printMessage(uint8_t line, const char *text, uint8_t startX) {
    if (m_msgQueue == nullptr || text == nullptr) return;

    DisplayMessage msg = {};
    msg.targetLine = line;
    msg.startX = (startX > 127) ? 127 : startX; // Clamp to maximum physical display width

    // Thread-safe copy buffer sanitization sequence
    strncpy(msg.content, text, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';

    // Asynchronous non-blocking dispatch push (bails instantly if full to prevent lockups)
    xQueueSend(m_msgQueue, &msg, 0);
}

/**
 * @brief Static trampoline layer connecting the native C-based FreeRTOS scheduler directly back to our C++ runtime context.
 * @param pvParameters Raw pointer mapping the reference context of the current instance tracking space.
 */
void DisplayController::taskTrampoline(void *pvParameters) {
    auto *instance = static_cast<DisplayController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

/**
 * @brief Autonomous main task routing layer parsing compound UTF-8 string inputs and enforcing real-time automatic end-of-line wiping.
 */
void DisplayController::run() {
    // Perform hardware initialization safely encapsulated inside the unique thread sequence boundary
    esp_err_t init_status = initializeHardware();
    if (init_status != ESP_OK) {
        ESP_LOGE(TAG, "Hardware setup failed completely. Execution layer aborted.");
        return;
    }

    ESP_LOGI(TAG, "SSD1306 display initialized and listening for incoming render packages.");

    DisplayMessage incoming = {};

    while (true) {
        // Asynchronous structural block yield: consumes zero CPU until a new message package arrives
        if (xQueueReceive(m_msgQueue, &incoming, portMAX_DELAY) == pdTRUE) {

            // Extract the Auto-Clear suppression flag from the MSB (Bit 7) and mask the line index
            const bool disableAutoClear = incoming.targetLine & 0x80;
            const uint8_t cleanLine = incoming.targetLine & 0x07; // Native 0-7 clamp using bitmask

            // POSITIONING THE HARDWARE POINTER
            sendCommand(static_cast<uint8_t>(0xB0 + cleanLine)); // Set page address boundary

            // DYNAMIC HARDWARE OFFSET INJECTION
            // Classic TTGO starts at column 0, while T-Beam Supreme (SH1106/7 layout) requires a +2 pixel shift
#if defined(CONFIG_IDF_TARGET_ESP32S3)
            uint8_t correctedX = static_cast<uint8_t>(incoming.startX + 2);
            uint8_t displayWidth = 132; // SH1106 RAM width
#else
            uint8_t correctedX = incoming.startX;
            uint8_t displayWidth = 128; // SSD1306 RAM width
#endif

            // Set the horizontal column start pointer (startX) elegantly without magic 0x00 OR-mask
            sendCommand(static_cast<uint8_t>(correctedX & 0x0F));
            sendCommand(static_cast<uint8_t>(0x10 | (correctedX >> 4)));

            // Track absolute pixel position starting from the custom X coordinate
            uint8_t totalBytesWritten = correctedX;
            const size_t textLength = strlen(incoming.content);

            for (size_t i = 0; i < textLength; i++) {
                const uint8_t c_primary = static_cast<uint8_t>(incoming.content[i]);
                size_t fontIndex = 0;
                bool validCharacter = false;
                const size_t current_char_idx = i; // Store original index for connection logic

                // Multi-byte UTF-8 decoder targeting the complete ISO-8859-1 (Latin-1) spectrum
                if ((c_primary == 0xC3 || c_primary == 0xC2) && (i + 1) < textLength) {
                    const uint8_t c_secondary = static_cast<uint8_t>(incoming.content[++i]);

                    // Mathematical reconstruction of the raw Latin-1 byte position
                    // 0xC3 sequence maps to 0xC0-0xFF (c_secondary + 0x40)
                    // 0xC2 sequence maps directly to 0x80-0xBF
                    uint8_t iso_char = (c_primary == 0xC3) ? (c_secondary + 0x40) : c_secondary;

                    // Subtract table offset since font array starts at index 32 (Space)
                    fontIndex = static_cast<size_t>(iso_char - 32);
                    validCharacter = true;
                }
                // Handle standard printable 7-bit ASCII characters
                else if (c_primary >= 32 && c_primary <= 126) {
                    fontIndex = static_cast<size_t>(c_primary - 32);
                    validCharacter = true;
                }

                if (validCharacter && totalBytesWritten < static_cast<uint8_t>(displayWidth - 6)) {
                    uint8_t txBuffer[7];
                    // copy all 5 font columns in a single, hardware-accelerated burst
                    txBuffer[0] = 0x40; // Data identifier
                    // txBuffer[1] = font_5x7[fontIndex][0];
                    // txBuffer[2] = font_5x7[fontIndex][1];
                    // txBuffer[3] = font_5x7[fontIndex][2];
                    // txBuffer[4] = font_5x7[fontIndex][3];
                    // txBuffer[5] = font_5x7[fontIndex][4];
                    std::memcpy(&txBuffer[1], font_5x7[fontIndex], 5);

                    // Check if current character is a potential line segment
                    if (c_primary == '-' || c_primary == '_' || c_primary == '=') {
                        // Look behind: Direct boolean evaluation in a single line
                        bool connect = (current_char_idx > 0 && incoming.content[current_char_idx - 1] == static_cast<char>(c_primary));

                        // Look ahead: Only checks if the look-behind didn't already establish a connection
                        if (!connect && (i + 1) < textLength && incoming.content[i + 1] == static_cast<char>(c_primary)) {
                            connect = true;
                        }

                        // Seamlessly bridge the 1-pixel gap if part of a multi-character line sequence
                        txBuffer[6] = connect ? txBuffer[5] : 0x00;
                    } else {
                        txBuffer[6] = 0x00; // Standard 1-pixel spacing for regular text elements
                    }

                    if (m_devHandle != nullptr) {
                        i2c_master_transmit(m_devHandle, txBuffer, sizeof(txBuffer), 50);
                    }
                    totalBytesWritten = static_cast<uint8_t>(totalBytesWritten + 6);
                }
            }

            // AUTO-CLEAR: Fill the remaining horizontal page width with blank pixels only if flag is NOT active
            if (!disableAutoClear && totalBytesWritten < 128) {
                const uint8_t remainingBytes = static_cast<uint8_t>(128 - totalBytesWritten);

                // Allocate a burst buffer with dynamic sizing plus control byte room
                uint8_t flushBuffer[129] = {0};
                flushBuffer[0] = 0x40; // Data mode identifier
                if (m_devHandle != nullptr) {
                    // Send out the precise amount of zeros required to wipe the right margin clean
                    i2c_master_transmit(m_devHandle, flushBuffer, static_cast<size_t>(remainingBytes + 1), 50);
                }
            }
        }
    }
}

/**
 * @brief Run a comprehensive visual test suite and clear the screen after 5 seconds.
 * @param display Pointer to the active DisplayController instance.
 */
void run_display_font_test(DisplayController* display) {
    if (display == nullptr) return;

    // Test 1: Standard printable ASCII characters (Lines 0 and 1)
    display->printMessage(0, " ",0); // clear line
    display->printMessage(0, "=== FONT TEST ===", 12);
    display->printMessage(1, "ASCII: abcXYZ 123!?", 0);

    // Test 2: Native German Umlaute via multi-byte UTF-8 (Lines 2, 3 and 4)
    display->printMessage(2, "Umlaute: AE-OE-UE-ss", 0);
    display->printMessage(3, "Outputs: Ä  Ö  Ü  ß", 0);
    display->printMessage(4, "Outputs: ä  ö  ü", 0);

    // Test 3: Newly supported Latin-1 symbols (Line 5)
    display->printMessage(5, "Symbols: 23°C © 2026", 0);

    // Test 4: Dynamic line-segment connection logic (Lines 6 and 7)
    display->printMessage(6, "--------------------", 0);
    display->printMessage(7, "Line test: ___ === _", 0);

    // Let the user look at the test pattern for 60 seconds
    vTaskDelay(pdMS_TO_TICKS(60000));
    // Display löschen oder Zeilen vorbereiten, falls nötig
    for (uint8_t line = 0; line < 8; line++) {
        display->printMessage(line, " "); // Zeile leeren
    }
    int c = 32;
    for ( uint8_t line = 0; line < 8 ; line++) {
        for ( uint8_t i = 0; i < 22 ; i++ ) {
            if (c > 255) break;

            char buf[2] = { (char)c, '\0' };

            display->appendMessage(line, buf , i);

            c++; //next char
        }
    }
    vTaskDelay(pdMS_TO_TICKS(60000));
    // Print a quick shutdown notice to the CLI console before restarting
    printf("\r\n[SYSTEM] Visual test completed. Triggering hardware reset now...\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Perform a clean software-controlled hardware restart of the ESP32
    esp_restart();
     // Clear all lines by printing empty strings (triggers Auto-Clear)
     // for (uint8_t line = 0; line < 8; line++) {
     //     display->printMessage(line, "", 0);
     // }
}
