#!/usr/bin/env python3
"""Genera la tabla del tipo de letra 5x7 usada por el HUD de OpenGL.

Cada glifo se escribe como arte ASCII de 7 filas por 5 columnas y se codifica
en 5 bytes: un byte por columna, con el bit 0 en la fila superior.
"""
import textwrap

G = {}


def g(ch, art):
    rows = [r for r in art.strip("\n").split("\n")]
    assert len(rows) == 7, (ch, len(rows))
    rows = [(r + "     ")[:5] for r in rows]
    cols = []
    for c in range(5):
        byte = 0
        for r in range(7):
            if rows[r][c] not in " .":
                byte |= 1 << r
        cols.append(byte)
    G[ch] = cols


g(" ", "\n".join(["....."] * 7))
g("!", """
..#..
..#..
..#..
..#..
..#..
.....
..#..
""")
g("%", """
##...
##..#
...#.
..#..
.#...
#..##
...##
""")
g("(", """
...#.
..#..
.#...
.#...
.#...
..#..
...#.
""")
g(")", """
.#...
..#..
...#.
...#.
...#.
..#..
.#...
""")
g("*", """
.....
..#..
#.#.#
.###.
#.#.#
..#..
.....
""")
g("+", """
.....
..#..
..#..
#####
..#..
..#..
.....
""")
g(",", """
.....
.....
.....
.....
.....
..##.
..#..
""")
g("-", """
.....
.....
.....
#####
.....
.....
.....
""")
g(".", """
.....
.....
.....
.....
.....
.##..
.##..
""")
g("/", """
....#
....#
...#.
..#..
.#...
#....
#....
""")
g("0", """
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
""")
g("1", """
..#..
.##..
..#..
..#..
..#..
..#..
.###.
""")
g("2", """
.###.
#...#
....#
...#.
..#..
.#...
#####
""")
g("3", """
#####
...#.
..#..
...#.
....#
#...#
.###.
""")
g("4", """
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
""")
g("5", """
#####
#....
####.
....#
....#
#...#
.###.
""")
g("6", """
..##.
.#...
#....
####.
#...#
#...#
.###.
""")
g("7", """
#####
....#
...#.
..#..
.#...
.#...
.#...
""")
g("8", """
.###.
#...#
#...#
.###.
#...#
#...#
.###.
""")
g("9", """
.###.
#...#
#...#
.####
....#
...#.
.##..
""")
g(":", """
.....
.##..
.##..
.....
.##..
.##..
.....
""")
g("<", """
...#.
..#..
.#...
#....
.#...
..#..
...#.
""")
g("=", """
.....
.....
#####
.....
#####
.....
.....
""")
g(">", """
.#...
..#..
...#.
....#
...#.
..#..
.#...
""")
g("?", """
.###.
#...#
....#
...#.
..#..
.....
..#..
""")
g("[", """
.###.
.#...
.#...
.#...
.#...
.#...
.###.
""")
g("]", """
.###.
...#.
...#.
...#.
...#.
...#.
.###.
""")
g("_", """
.....
.....
.....
.....
.....
.....
#####
""")
g("|", """
..#..
..#..
..#..
..#..
..#..
..#..
..#..
""")
g("A", """
.###.
#...#
#...#
#####
#...#
#...#
#...#
""")
g("B", """
####.
#...#
#...#
####.
#...#
#...#
####.
""")
g("C", """
.###.
#...#
#....
#....
#....
#...#
.###.
""")
g("D", """
###..
#..#.
#...#
#...#
#...#
#..#.
###..
""")
g("E", """
#####
#....
#....
####.
#....
#....
#####
""")
g("F", """
#####
#....
#....
####.
#....
#....
#....
""")
g("G", """
.###.
#...#
#....
#.###
#...#
#...#
.####
""")
g("H", """
#...#
#...#
#...#
#####
#...#
#...#
#...#
""")
g("I", """
.###.
..#..
..#..
..#..
..#..
..#..
.###.
""")
g("J", """
..###
...#.
...#.
...#.
...#.
#..#.
.##..
""")
g("K", """
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
""")
g("L", """
#....
#....
#....
#....
#....
#....
#####
""")
g("M", """
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
""")
g("N", """
#...#
#...#
##..#
#.#.#
#..##
#...#
#...#
""")
g("O", """
.###.
#...#
#...#
#...#
#...#
#...#
.###.
""")
g("P", """
####.
#...#
#...#
####.
#....
#....
#....
""")
g("Q", """
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
""")
g("R", """
####.
#...#
#...#
####.
#.#..
#..#.
#...#
""")
g("S", """
.####
#....
#....
.###.
....#
....#
####.
""")
g("T", """
#####
..#..
..#..
..#..
..#..
..#..
..#..
""")
g("U", """
#...#
#...#
#...#
#...#
#...#
#...#
.###.
""")
g("V", """
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
""")
g("W", """
#...#
#...#
#...#
#.#.#
#.#.#
##.##
#...#
""")
g("X", """
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
""")
g("Y", """
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
""")
g("Z", """
#####
....#
...#.
..#..
.#...
#....
#####
""")

first = 32
last = 95  # hasta '_'
missing = [chr(c) for c in range(first, last + 1) if chr(c) not in G]
for ch in missing:
    G[ch] = [0, 0, 0, 0, 0]

lines = []
for code in range(first, last + 1):
    ch = chr(code)
    cols = G[ch]
    display = ch if ch not in ('\\',) else '\\\\'
    lines.append("    {{0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}}},  // '{}'".format(
        *cols, display))

body = "\n".join(lines)
header = f"""#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Tipo de letra de mapa de bits 5x7 generado por scripts/generate_font.py.
//
// Se incluye dentro del programa para poder dibujar los FPS y las estadisticas
// sobre la escena sin depender de SDL_ttf ni de archivos externos: el proyecto
// debe entregarse como codigo fuente compilable en cualquier maquina.
//
// Cada glifo son 5 bytes, uno por columna. El bit 0 corresponde a la fila
// superior y el bit 6 a la inferior.
// ---------------------------------------------------------------------------
namespace font5x7 {{

constexpr int GLYPH_WIDTH = 5;    // Columnas por glifo.
constexpr int GLYPH_HEIGHT = 7;   // Filas por glifo.
constexpr char FIRST_CHAR = {first};  // Primer caracter representado (espacio).
constexpr char LAST_CHAR = {last};   // Ultimo caracter representado ('_').

// Tabla de glifos indexada por (caracter - FIRST_CHAR).
constexpr std::uint8_t GLYPHS[][GLYPH_WIDTH] = {{
{body}
}};

// Devuelve el glifo correspondiente al caracter, convirtiendo minusculas a
// mayusculas y sustituyendo por espacio cualquier caracter fuera de la tabla.
inline const std::uint8_t* glyphFor(char character) {{
    if (character >= 'a' && character <= 'z') {{
        character = static_cast<char>(character - 'a' + 'A');
    }}
    if (character < FIRST_CHAR || character > LAST_CHAR) {{
        character = ' ';
    }}
    return GLYPHS[static_cast<int>(character) - FIRST_CHAR];
}}

}}  // namespace font5x7
"""
open("include/Font5x7.h", "w").write(header)
print(f"include/Font5x7.h generado con {last - first + 1} glifos")
