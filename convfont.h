#pragma once

#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef _MSC_VER
#define noreturn __declspec(noreturn)
#else
#ifdef __GNUC__
#define noreturn __attribute__((noreturn))
#else
#define noreturn _Noreturn
#endif
#endif

#define MAX_APPVAR_SIZE 0xFFE8
#define MEATADATA_STRUCT_SIZE 21

extern int verbosity;

typedef enum {
    output_unspecified = 0,
    output_fontpack,
    output_c_array,
    output_asm_array,
    output_binary_blob,
} output_formats_t;

typedef enum {
    bad_infile = 1,
    bad_outfile,
    invalid_fnt,
    bad_options,
    malloc_failed,
    internal_error,
    text_parser_error,
} error_codes_t;

/* This definition just adds some typing to otherwise opaque chunks of data. */
typedef struct {
    int length;
    uint8_t bytes[1];
} fontlib_bitmap_t;

typedef struct {
    /* Must be zero */
    uint8_t fontVersion;
    /* Height in pixels */
    uint8_t height;
    /* Total number of glyphs provided. */
    uint16_t total_glyphs;
    /* Number of first glyph.  If you have no codepoints below 32, for
    example, you can omit the first 32 bitmaps. */
    uint8_t first_glyph;
    /* Offset/pointer to glyph widths table.
    * This is an OFFSET from the fontVersion member in data format.
    * However, it is 24-bits long because it becomes a real pointer upon loading. */
    uint8_t *widths_table;
    /* Offset to a table of offsets to glyph bitmaps.
    * These offsets are only 16-bits each to save some space. */
    fontlib_bitmap_t **bitmaps;
    /* Specifies how much to move the cursor left after each glyph.
    Total movement is width - overhang.  Intended for italics. */
    uint8_t italic_space_adjust;
    /* These suggest adding blank space above or below each line of text.
    This can increase legibility. */
    uint8_t space_above;
    uint8_t space_below;
    /* Specifies the boldness of the font.
    0x40: light
    0x80: regular
    0x90: medium
    0xC0: bold*/
    uint8_t weight;
    /* Specifies the style of the font.  See enum font_styles */
    uint8_t style;
    /* For layout, allows aligning text of differing fonts vertically.
    These count pixels going down, i.e. 0 means the top of the glyph. */
    uint8_t cap_height;
    uint8_t x_height;
    uint8_t baseline_height;
} fontlib_font_t;

/*uint8_t get_cols(uint8_t width)
{
return ((width - 1) >> 3) + 1;
}*/

/* This macro provides a less ugly means of computing the number of bytes per
 * row for a glyph. */
#define byte_columns(width) ((((width) - 1) >> 3) + 1)

/**
 * Displays an error and exits.
 */
noreturn void throw_error(const int code, const char *string);

/**
* Displays an error and exits.
*/
noreturn void throw_errorf(const int code, const char *string, ...);

/**
* Displays an error and exits.
*/
noreturn void vthrow_errorf(const int code, const char *string, va_list args);



/*******************************************************************************
*                             STYLES AND WEIGHTS                               *
*******************************************************************************/

typedef struct {
    char* string;
    uint8_t value;
} string_value_pair_t;

typedef struct {
    int count;
    string_value_pair_t *strings;
} string_list_t;

/**
 * Compares a string against a list of strings and numeric values to associate
 * with that string.  Returns -1 if no match is found.
 */
int check_string_for_value(char *string, string_list_t *possible_values);

/**
 * Compares two strings, ignoring case.
 */
bool strcaseeq(const char *str1, const char *str2);

/**
 * List of names of styles.
 */
extern string_list_t styles;

/**
* List of names of weights.
*/
extern string_list_t weights;