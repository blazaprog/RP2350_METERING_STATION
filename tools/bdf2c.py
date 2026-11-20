#!/usr/bin/env python3
# ============================================================================
#  bdf2c.py - převede bitmapové písmo BDF na pole v jazyce C (src/oled_fonts.c)
#
#  Použití:  python3 tools/bdf2c.py spleen-5x8.bdf spleen-8x16.bdf ... > src/oled_fonts.c
#  Do výstupu se vezmou znaky ASCII 32 až 126 a navíc znak stupně (U+00B0).
#  Každý znak je buňka w x h pixelů; jeden řádek buňky zabírá (w+7)/8 bajtů,
#  nejlevější pixel je nejvyšší bit.
# ============================================================================
import re, sys, os

CODES = list(range(32, 127)) + [0xB0]          # pořadí znaků ve výstupní tabulce

def parse_bdf(path):
    """Načte BDF a vrátí (šířka buňky, výška buňky, {kód: [řádky jako čísla]})."""
    glyphs, cur, in_bitmap = {}, None, False
    ascent = descent = None
    fbb = None
    for line in open(path, encoding="latin-1"):
        t = line.split()
        if not t: continue
        if t[0] == "FONT_ASCENT": ascent = int(t[1])
        elif t[0] == "FONT_DESCENT": descent = int(t[1])
        elif t[0] == "FONTBOUNDINGBOX": fbb = tuple(int(v) for v in t[1:5])
        elif t[0] == "STARTCHAR": cur = {"rows": []}
        elif t[0] == "ENCODING" and cur is not None: cur["enc"] = int(t[1])
        elif t[0] == "BBX" and cur is not None: cur["bbx"] = tuple(int(v) for v in t[1:5])
        elif t[0] == "BITMAP": in_bitmap = True
        elif t[0] == "ENDCHAR":
            in_bitmap = False
            if cur.get("enc", -1) >= 0: glyphs[cur["enc"]] = cur
            cur = None
        elif in_bitmap: cur["rows"].append(t[0])
    w, h = fbb[0], fbb[1]
    out = {}
    for code, g in glyphs.items():
        gw, gh, xo, yo = g["bbx"]
        cell = [0] * h
        top = ascent - (yo + gh)                 # řádek buňky, kde začíná bitmapa znaku
        for i, hexrow in enumerate(g["rows"]):
            bits = int(hexrow, 16) << (len(hexrow) * 4 * 0)   # řádek jako číslo
            nbits = len(hexrow) * 4
            for x in range(gw):
                if bits >> (nbits - 1 - x) & 1:
                    cx = x + xo - fbb[2]
                    ry = top + i
                    if 0 <= cx < w and 0 <= ry < h: cell[ry] |= 1 << (w - 1 - cx)
        out[code] = cell
    return w, h, out

def emit(path):
    w, h, glyphs = parse_bdf(path)
    bpr = (w + 7) // 8
    name = f"font_{w}x{h}_data"
    print(f"static const uint8_t {name}[] = {{")
    for code in CODES:
        cell = glyphs.get(code, [0] * h)
        label = chr(code) if code != 0xB0 else "stupen"
        # každý řádek rozdělíme na bajty (u znaků širších než 8 pixelů jsou to 2 a více bajtů na řádek)
        row = ", ".join("0x%02X" % b for r in cell for b in (r << (bpr * 8 - w)).to_bytes(bpr, "big"))
        print(f"    {row},  // {label!r}" if code != 0xB0 else f"    {row},  // {label}")
    print("};")
    print(f"const oled_font_t font_{w}x{h} = {{ {w}, {h}, {name} }};\n")

print("// ============================================================================")
print("//  oled_fonts.c - bitmapová písma pro displej (VYGENEROVÁNO skriptem tools/bdf2c.py)")
print("//  Písmo: Spleen (c) 2018-2026 Frederic Cambus, licence BSD 2-Clause (lib/fonts/LICENSE-spleen.txt).")
print("//  Neupravuj ručně - při změně znovu spusť skript.")
print("// ============================================================================")
print('#include "oled.h"\n')
for p in sys.argv[1:]:
    emit(p)
