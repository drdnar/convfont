#pragma once

#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#include "convfont.h"

/**
 * Unpacks a font in text format into RAM.
 * @param input The already-opened file to read from.
 * @return A pointer to a malloc()ed font. */
fontlib_font_t *parse_text(FILE *input, char encoding);
