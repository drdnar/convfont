#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "convfont.h"
#include "parse_text.h"

enum {
    UNKNOWN_FONT_TAG,
    STYLE,
    HEIGHT,
    WEIGHT,
    ITALIC_ADJUST,
    CAP_HEIGHT,
    X_HEIGHT,
    BASELINE_HEIGHT,
    SPACE_ABOVE,
    SPACE_BELOW,
    DOUBLE_WIDTH_DEFAULT,
    DEFAULT_WIDTH,
    FIXED_WIDTH,
    GLYPH_DATA,
};

const string_value_pair_t font_tag_names[] =
{
    { "height", HEIGHT },
    { "weight", WEIGHT },
    { "style", STYLE },
    { "italicadjust", ITALIC_ADJUST },
    { "italicadjustment", ITALIC_ADJUST },
    { "italic adjust", ITALIC_ADJUST },
    { "italic adjustment", ITALIC_ADJUST },
    { "italic_adjust", ITALIC_ADJUST },
    { "italic_adjustment", ITALIC_ADJUST },
    { "cap", CAP_HEIGHT },
    { "capheight", CAP_HEIGHT },
    { "cap height", CAP_HEIGHT },
    { "cap_height", CAP_HEIGHT },
    { "x", X_HEIGHT },
    { "xheight", X_HEIGHT },
    { "x height", X_HEIGHT },
    { "x-height", X_HEIGHT },
    { "x_height", X_HEIGHT },
    { "baseline", BASELINE_HEIGHT },
    { "baselineheight", BASELINE_HEIGHT },
    { "baseline height", BASELINE_HEIGHT },
    { "baseline_height", BASELINE_HEIGHT },
    { "above", SPACE_ABOVE },
    { "spaceabove", SPACE_ABOVE },
    { "space above", SPACE_ABOVE },
    { "space_above", SPACE_ABOVE },
    { "below", SPACE_BELOW },
    { "spacebelow", SPACE_BELOW },
    { "space below", SPACE_BELOW },
    { "space_below", SPACE_BELOW },
    { "double", DOUBLE_WIDTH_DEFAULT },
    { "doublewidth", DOUBLE_WIDTH_DEFAULT },
    { "double width", DOUBLE_WIDTH_DEFAULT },
    { "double_width", DOUBLE_WIDTH_DEFAULT },
    { "defaultwidth", DEFAULT_WIDTH },
    { "default width", DEFAULT_WIDTH },
    { "default_width", DEFAULT_WIDTH },
    { "width", DEFAULT_WIDTH },
    { "fixedwidth", FIXED_WIDTH },
    { "fixed width", FIXED_WIDTH },
    { "fixed_width", FIXED_WIDTH },
    { "glyphs", GLYPH_DATA },
    { "characters", GLYPH_DATA },
    { "bitmaps", GLYPH_DATA },
    { "font data", GLYPH_DATA },
    { "data", GLYPH_DATA },
};

const string_list_t font_tags = {
    sizeof(font_tag_names) / sizeof(string_value_pair_t), font_tag_names
};

enum {
    UNKNOWN_GLYPH_TAG,
    CODEPOINT,
    DOUBLE_WIDTH,
    WIDTH,
    DATA,
};

const string_value_pair_t glyph_tag_names[] =
{
    { "double", DOUBLE_WIDTH },
    { "doublewidth", DOUBLE_WIDTH },
    { "double width", DOUBLE_WIDTH },
    { "double_width", DOUBLE_WIDTH },
    { "codepoint", CODEPOINT },
    { "code point", CODEPOINT },
    { "index", CODEPOINT },
    { "code", CODEPOINT },
    { "width", WIDTH },
    { "data", DATA },
    { "image", DATA },
    { "bitmap", DATA },
};

const string_list_t glyph_tags = {
    sizeof(glyph_tag_names) / sizeof(string_value_pair_t), glyph_tag_names
};

const string_value_pair_t bool_names[] =
{
    { "false", 0 },
    { "no", 0 },
    { "0", 0 },
    { "n", 0 },
    { "nope", 0 },
    { "nah", 0 },
    { "nay", 0 },
    { "true", 1 },
    { "yes", 1 },
    { "y", 1 },
    { "1", 1 },
    { "yup", 1 },
    { "yep", 1 },
    { "yeah", 1 },
    { "yea", 1 },
    { "sure", 1 },
};

const string_list_t bools = {
    sizeof(bool_names) / sizeof(string_value_pair_t), bool_names
};



/******************************************************************************/

#define MAX_LINE_LENGTH 256
#define ENCODING_ERROR          -11001
#define ENCODING_CONFLICT       -11002
#define BAD_NULL                -11003
#define NOT_EXPECTED            -11004
#define SYNTAX_ERROR            -11005
#define HEADER_ERROR            -11006
#define LINE_TOO_LONG           -11007
#define BAD_VALUE               -11008
#define BAD_METADATA			-11009
#define WIDTH_CONFLICT			-11010

/**
* Current state of newline processing.
*/
enum {
    IN_LINE,
    CR,
    LF
};

/**
* Encoding types
*/
enum {
    UNKNOWN, /**< Not sure how to deal with high-bit-set codes. */
    ASCII, /**< Force 8-bit code processing. */
    UTF8,
    UTF16BE,
    UTF16LE
};

/**
* UTF-16 state
*/
enum {
    START, /**< Expecting the start of a potential surrogate pair. */
    NEXT, /**< Expecting the continuation of a surrogate pair. */
    UNGOT /**< A previously read codepoint has been returned to the queue, so don't try to reparse it. */
};

/**
* Current state of file parser.
*/
typedef struct parser_state {
    FILE *file;
    /**
    * Tells get_next_char whether and how to parse multibyte codes.
    */
    char encoding;
    /**
    * For UTF-16 coding, this holds the state of the surrogate pair parser.
    */
    char expecting;
    int line_number;
    int line_pos;
    int line_len;
    char line[MAX_LINE_LENGTH + 1];
} parser_state_t;


#define ERROR(X) do { throw_errorf(text_parser_error, "Line %i: %s", state->line_number, X); } while (false)

#define NEXT_RAW(X) do { if ((X = fgetc(state->file)) < 0) return X; } while (false)
#define NEXT_RAW_THROW(X, Y) do { if ((X = fgetc(state->file)) < 0) ERROR(Y); } while (false)
#define NEXT(X) do { if ((X = get_next_char(state)) < 0) return X; } while (false)
#define NEXT_THROW(X, Y) do { if ((X = get_next_char(state)) < 0) ERROR(Y); } while (false)


/**
* Gets the next codepoint.
* @return int Negative on failure.
*/
static int get_next_char(parser_state_t *state) {
    long int c, c2;
    NEXT_RAW(c);
    if (state->encoding == UTF16BE || state->encoding == UTF16LE) {
        if (state->expecting == UNGOT) {
            state->expecting = START;
            return c;
        }
        NEXT_RAW_THROW(c2, "Failed to get second byte of UTF-16 pair.");
        if (state->encoding == UTF16BE)
            c = (c << 8) | c2;
        else
            c = (c2 << 8) | c;
        /* Consume surrogate pair if needed */
        if (c >= 0xD800 && c < 0xDC00) {
            if (state->expecting == NEXT)
                ERROR("Two first halves of surrogate pair.");
            state->expecting = NEXT;
            c2 = get_next_char(state);
            state->expecting = START;
            if (c2 < 0)
                return c2;
            c = 0x10000 + ((c2 & 0x3FF) | ((c & 0x3FF) << 10));
            /* Encoding a surrogate pair with a surrogate pair? */
            if (c >= 0xD800 && c < 0xE000)
                ERROR("Surrogate pair encoded with surrogate pair.");
        } else if (c >= 0xDC00 && c < 0xE000) {
            if (state->expecting == START)
                ERROR("Unexpected second half of surrogate pair.");
        } else if (state->expecting == NEXT)
            ERROR("Expected second half of surrogate pair.");
    } else if (state->encoding == UTF8) {
        if (c == 0)
            ERROR("Null in input text.");
        if (c >= 0xF5)
            ERROR("Invalid UTF-8 byte >= 0xF5.");
        /* Read multibyte sequence */
        if (c >= 0xC2) {
            int required_bytes;
            if (c >= 0xF0) {
                c &= 7;
                required_bytes = 3;
            } else if (c >= 0xE0) {
                c &= 0xF;
                required_bytes = 2;
            } else if (c >= 0xC2) {
                c &= 0x1F;
                required_bytes = 1;
            } else if (c >= 0x80)
                ERROR("Invalid UTF-8 byte.");
            while (required_bytes--> 0) {
                NEXT_RAW_THROW(c2, "Failed to get subsequent bytes of UTF-8 multibyte sequence.");
                if (c2 < 0x80 || c2 >= 0xC0)
                    ERROR("Expected subsequent UTF-8 byte.");
                c = (c << 6) | (c2 & 0x3F);
            }
            if (c >= 0xD800 && c < 0xE000)
                ERROR("Surrogate pair encoded in UTF-8.");
        }
    }
    if (c == 0)
        ERROR("Null in input text.");
    /* Technically, int is allowed to be 16 bits. */
    if ((INT_MAX <= 0x10FFFF) && c > INT_MAX)
        return INT_MAX;
    return c;
}


/**
* Returns a codepoint to the read buffer.
*/
static void unget(int c, parser_state_t *state) {
    state->expecting = UNGOT;
    ungetc(c, state->file);
}


/**
* Gets the next codepoint, and parses newlines.
* \r\n is collated into a single \n, and \r is also converted into \n.
* Unicode spaces are converted into simple spaces, and zero-width characters are skipped.
* @return int Negative on failure.
*/
static int get_next(parser_state_t *state) {
    int c;
read_again:
    NEXT(c);
    if (state->encoding > ASCII) {
        switch (c) {
            /* Convert things like spaces into actual spaces */
            case 0xA0: /* Non-breaking space */
            case 0x1680: /* Ogham space mark */
            case 0x2000: /* En quad */
            case 0x2002: /* En space */
            case 0x2001: /* Em quad */
            case 0x2003: /* Em space */
            case 0x2004:
            case 0x2005:
            case 0x2006:
            case 0x2007:
            case 0x2008: /* Punctuation space */
            case 0x202F: /* Narrow no-break space */
            case 0x205F: /* Medium mathematical space */
            case 0x2012: /* Figure dash (same width as digits 0-9) */
            case 0x2013: /* En dash */
            case 0x2014: /* Em dash */
            case 0x2212: /* Minus sign (same width as +) */
            case 0x3000: /* Ideographic space */
                c = ' ';
                break;
                /* Ignore these and just read something else */
            case 0xAD: /* Soft hyphen */
            case 0x2009: /* Thin space */
            case 0x200A: /* Hair space */
            case 0x200B: /* Zero width space */
            case 0x200C: /* Zero width non-joiner */
            case 0x200D: /* Zero width joiner */
            case 0x2060: /* Word joiner */
            case 0xFEFF: /* Zero width non-breaking space or Byte order mark */
                goto read_again;
        }
    }
    if (c == '\n')
        return c;
    if (c != '\r')
        return c;
    NEXT(c);
    if (c != '\n')
        unget(c, state);
    return '\n';
}


/**
* Initialize file parsing.
*
* @param state Pointer to state object
* @param file FILE to use for input
* @param encoding
*/
static void init_parser(parser_state_t *state, FILE *file, char encoding) {
    state->file = file;
    state->expecting = START;
    state->encoding = encoding;
    state->line_number = 1;
    int c, c2;
    NEXT_RAW_THROW(c, "Out of bytes in header.");
    NEXT_RAW_THROW(c2, "Out of bytes in header.");
    if (c >= 0xFE) {
        if (c == 0xFE && c2 == 0xFF)
            if (encoding == UTF16BE || encoding == UNKNOWN)
                state->encoding = UTF16BE;
            else
                ERROR("Requested encoding conflicts with BOM detected.");
        else if (c == 0xFF && c2 == 0xFE)
            if (encoding == UTF16LE || encoding == UNKNOWN)
                state->encoding = UTF16LE;
            else
                ERROR("Requested encoding conflicts with BOM detected.");
        else
            ERROR("BOM-like bytes not valid.");
        if (get_next_char(state) != 'c')
            ERROR("File does not start with \'convfont\'.");
    } else if (c == 0xEF) {
        if (c2 != 0xBB)
            ERROR("File does not start with \'convfont\'.");
        if ((c2 = fgetc(state->file)) < 0)
            ERROR("Out of bytes in header.");
        if (c2 != 0xBF)
            ERROR("File does not start with \'convfont\'.");
        if (encoding == UTF8 || encoding == UNKNOWN)
            state->encoding = UTF8;
        else
            ERROR("File starts with UTF-8 BOM, but requested different encoding.");
    } else {
        if (c == 'c' && c2 == '\0')
            if (encoding == UTF16LE || encoding == UNKNOWN)
                state->encoding = UTF16LE;
            else
                ERROR("File does not start with \'convfont\'.");
        else if (c == '\0' && c2 == 'c')
            if (encoding == UTF16BE || encoding == UNKNOWN)
                state->encoding = UTF16BE;
            else
                ERROR("File does not start with \'convfont\'.");
        else if (c == 'c') {
            if (encoding != UNKNOWN)
                state->encoding = UTF8;
            ungetc(c2, state->file);
        } else
            ERROR("File does not start with \'convfont\'.");
    }
    char eader[] = "onvfont";
    for (int i = 0; i < strlen(eader); i++) {
        NEXT_THROW(c, "Out of bytes in header.");
        if (eader[i] != c)
            ERROR("File does not start with \'convfont\'.");
    }
    if (get_next(state) != '\n')
        ERROR("File does not start with \'convfont\' and newline.");
    if (state->encoding == UNKNOWN)
        state->encoding = UTF8;
}


/**
* Buffers the next line of text.
* @return int >= 0 on success, negative on failure.
* Returns LINE_TOO_LONG if the line is too long, and eats the rest of the line.
* Returns EOF if no characters can be read because the file is done.
*/
static int get_next_line(parser_state_t *state) {
    int c;
    int len = MAX_LINE_LENGTH;
    state->line_number++;
    state->line_pos = 0;
    state->line_len = 0;
    char* buffer = state->line;
    do {
        if (--len <= 0) {
            *buffer = '\0';
            while (c != '\n' && c > 0)
                c = get_next(state);
            return LINE_TOO_LONG;
        }
        if ((c = get_next(state)) < 0) {
            *buffer = '\0';
            if (state->line_len < 1)
                return c;
            return 0;
        }
        if (c == '\n') {
            *buffer = '\0';
            return 0;
        }
        if (c > 0xFF)
            c = 0xFF;
        state->line_len++;
        *buffer++ = (char)c;
    } while (true);
}


/**
* Check if a character is whitespace.
*/
static bool is_whitespace(char ch) {
    return ch == ' ' || ch == '\t';
}


/**
* Skips over a bunch of whitespace.
*
* @return Pointer to first non-whitespace character.
*/
static char *eat_whitespace(char *str) {
    while (is_whitespace(*str))
        str++;
    return str;
}


/**
* Extracts a tag or data field from a line of text.
*
* @param input_string Pointer to input string
* @param mode False if looking for a tag (terminated with :), true if looking for value.
* @param output Pointer to output string.  This will be become NULL terminated.
* @param max_len Size of output string buffer.
* @return int Number of characters consumed.  Negative value on error.
* EOF means input ended before ':'.  LINE_TOO_LONG means out of output space.
*/
static int extract_field(char *input_string, bool mode, char *output, int max_len) {
    if (max_len <= 0) /* ? ? ? */
        throw_error(internal_error, "extract_field: max_len <= 0");
    /* Eat leading whitespace */
    char *input = eat_whitespace(input_string);
    char c = *input;
    /* You should never do this. */
    if (max_len == 1) {
        *output = '\0';
        if ((!mode && c == ':') || (mode && c == '\0'))
            return (int)(input - input_string);
        else
            return EOF;
    }
    /* Extract data */
    do {
        if (max_len-- <= 0)
            break;
        c = *input++;
        if (c == ':' && !mode)
            break;
        if (c == '\0') {
            if (mode)
                break;
            else {
                *output = '\0';
                return EOF;
            }
        }
        if (is_whitespace(c)) {
            *output++ = ' ';
            input = eat_whitespace(input);
            continue;
        }
        *output++ = c;
    } while (true);
    if (max_len < 0) {
        /* We're here because there isn't even space for the null terminator. */
        return LINE_TOO_LONG;
    }
    *output-- = '\0';
    /* Eat any possible trailing whitespace */
    if (*output == ' ')
        *output = '\0';
    return (int)(input - input_string);
}


/**
 * If char is hexadecimal, finds its value.
 * @return 0-15 for hexadecimal; 16 if not hexadecimal.
 */
static char dehexify(char x) {
    if (x >= '0' && x <= '9')
        return x - '0';
    else if (x >= 'A' && x <= 'F')
        return x - 'A' + 10;
    else if (x >= 'a' && x <= 'f')
        return x - 'a' + 10;
    else
        return 16;
}


/**
* Extracts some kind of number-like thing, mostly following C-like rules.
* Negative values are not allowed.
*/
static int get_number(parser_state_t *state, char **str) {
    char *string = eat_whitespace(*str);
    char c = *string;
    char next = '\0';
    int i = 0;
    switch (c) {
        case '\'':
            next = '\'';
        case '\"':
            c = *++string;
            if (c == '\0' || c == '\n')
                ERROR("Invalid char literal.");
            string++;
            if (c == '\\') {
                c = *string++;
                switch (c) {
                    case '\0': case '\n':
                        ERROR("Invalid char literal.");
                    case 'a': i = '\a'; break;
                    case 'b': i = '\b'; break;
                    case 'f': i = '\f'; break;
                    case 'n': i = '\n'; break;
                    case 'r': i = '\r'; break;
                    case 't': i = '\t'; break;
                    case 'v': i = '\v'; break;
                    case 'x':
                        if ((c = dehexify(*string)) > 15)
                            ERROR("Invalid hexadecimal escape sequence in char literal.");
                        for (i = 0; c <= 15; c = dehexify(*++string))
                            i = (i << 4) | c;
                        break;
                    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
                        i = c - '0';
                        c = *string;
                        if (c < '0' || c >= '8')
                            break;
                        i = (i << 3) | (c - '0');
                        c = *++string;
                        if (c < '0' || c >= '8')
                            break;
                        string++;
                        i = (i << 3) | (c - '0');
                        break;
                    case 'U':
#define NEXT_HEX_DIGIT(X) if ((c = dehexify(X)) > 15) ERROR("Invalid Unicode escape sequence in char literal."); i = (i << 4) | c;
                        NEXT_HEX_DIGIT(c);
                        NEXT_HEX_DIGIT(*string++);
                        NEXT_HEX_DIGIT(*string++);
                        NEXT_HEX_DIGIT(*string++);
                        c = *string++;
                    case 'u': /* Intentional fall-through */
                        NEXT_HEX_DIGIT(c);
                        NEXT_HEX_DIGIT(*string++);
                        NEXT_HEX_DIGIT(*string++);
                        NEXT_HEX_DIGIT(*string++);
                        break;
                    default:
                        i = (unsigned char)c;
                        break;
                }
                if (i < 0)
                    i = (((unsigned int)-1) >> 1) & i;
            } else
                i = (unsigned char)c;
            if ((next == '\'' && *string == '\'') || (next != '\'' && *string == '\"')) {
                *str = string;
                return i;
            }
            ERROR("Invalid char literal.");
        case 'U': case 'u':
            if (*++string != '+')
                ERROR("Invalid Unicode identifier.");
        case '$': /* Intentional fall-through */
            string++;
            while ((c = dehexify(*++string)) < 16)
                i = (i << 4) | c;
            *str = string;
            return i;
        case '0':
            c = tolower(*++string);
            if (c == 'x') {
                if ((c = dehexify(*++string)) > 15)
                    ERROR("Invalid hexadecimal number.");
                i = c;
                while ((c = dehexify(*++string)) < 16)
                    i = (i << 4) | c;
                *str = string;
                return i;
            } else if (c < '0' || c > '9') {
                *str = string;
                return 0;
            }
        case '1': case '2': case '3': case '4': /* Intentional fall-through */
        case '5': case '6': case '7': case '8': case '9':
            do {
                i = (i * 10) + (c - '0');
                c = *++string;
            } while (c >= '0' && c <= '9');
            *str = string;
            return i;
    }
    ERROR("Invalid number.");
    return -1;
}


typedef struct {
    /**
     * Pixels
     */
    uint32_t bitmap;
    /**
     * Number of pixels actually read into bitmap field.
     */
    uint_fast8_t width;
} bitmap_line_t;

/**
 * Parses a line of text as a bitmap.
 */
static bitmap_line_t prase_bitmap(char *text, bool double_width) {
    bitmap_line_t line = { 0, 0 };
    int width = 0;
    uint32_t bit = 0x80000000;
    for (char c = *text++; c != '\0'; c = *text++) {
        if (c != '0' && c != ' ')
            line.bitmap |= bit;
        bit >>= 1;
        line.width++;
        if (double_width && *text++ == '\0')
            break;
    }
    return line;
}


/**
 * Displays a bitmap.
 */
static void print_bitmap(uint32_t bitmap, int width) {
    while (width--> 0) {
        if (bitmap & 0x80000000)
            printf("1");
        else
            printf("0");
        bitmap <<= 1;
    }
}


static void is_error(parser_state_t *state, int val) {
    if (val >= 0)
        return;
    switch (val) {
        case NOT_EXPECTED:
            ERROR("Unexpected data in input.");
            break;
        case SYNTAX_ERROR:
            ERROR("Syntax error.");
            break;
        case LINE_TOO_LONG:
            ERROR("Line too long.");
            break;
        case BAD_VALUE:
            ERROR("Invalid value.");
            break;
        case EOF:
            ERROR("Unexpected end of data.");
            break;
        case BAD_METADATA:
            ERROR("Invalid metadata.");
            break;
        case WIDTH_CONFLICT:
            ERROR("Conflicting width data.");
            break;
        default:
            throw_error(internal_error, "Unhandled error code.");
            break;
    }
}

#define CHECK_FOR_ERROR(X) is_error(state, X)


/**
 * Parses a text-based font.
 * @param input The already-opened file to read from.
 * @return A pointer to a malloc()ed font.
 */
fontlib_font_t *parse_text(FILE *in_file, char encoding) {
    fontlib_font_t *target = malloc(sizeof(fontlib_font_t));
    if (!target)
        throw_error(malloc_failed, "parse_file: Failed to malloc fontlib_font_t.");
    target->widths_table = calloc(256, sizeof(uint8_t));
    if (!target->widths_table)
        throw_error(malloc_failed, "parse_file: Failed to calloc widths table.");
    target->bitmaps = calloc(256, sizeof(fontlib_bitmap_t *));
    if (!target->bitmaps)
        throw_error(malloc_failed, "parse_file: Failed to calloc bitmaps table.");
    target->fontVersion = 0;
    target->italic_space_adjust = target->space_above = target->space_below = 0;
    target->weight = target->style = 0;
    target->cap_height = target->x_height = target->baseline_height = 0;
    static parser_state_t state_var;
    parser_state_t *state = &state_var;
    /**
     * Cache of glyph bitmap prior to being properly serialized.
     */
    static uint32_t glyph_data[32];
    /**
     * Width of glyph currently being read.
     */
    int glyph_width;
    char tag[MAX_LINE_LENGTH];
    char val[MAX_LINE_LENGTH];
    char *str;
    int tag_length;
    int line;
    bool default_double_width = false;
    bool double_width;
    bool fixed_width = false;
    int default_width = -1;
    /**
     *
     */
    int width = -1;
    int height = -1;
    int italic_space_adjust = -1;
    int cap_height = -1;
    int x_height = -1;
    int baseline_height = -1;
    int space_above = -1;
    int space_below = -1;
    int style = -1;
    int weight = -1;
    /**
     * Current code point being read.
     */
    int codepoint = 0;
    /**
     * Number of glyphs read.
     */
    int count = 0;
    /**
     * First glyph number defined.
     */
    int first_glyph = 256;
    
    /**
     * Result
     */
    int r;
    init_parser(state, in_file, encoding);
    do {
        r = get_next_line(state);
        if (r == LINE_TOO_LONG && *eat_whitespace(state->line) == ':')
            continue;
        CHECK_FOR_ERROR(r);
        r = *eat_whitespace(state->line);
        if (r == ':' || r == '\0')
            continue;
        tag_length = extract_field(state->line, false, tag, MAX_LINE_LENGTH);
        if (tag_length == EOF)
            ERROR("Tag expected.");
        CHECK_FOR_ERROR(tag_length);
        r = extract_field(state->line + tag_length, true, val, MAX_LINE_LENGTH);
        CHECK_FOR_ERROR(r);
        r = check_string_for_value(tag, &font_tags);
        if (r < 0)
            continue;
        if (r == GLYPH_DATA)
            break;
        switch (r) {
            case HEIGHT:
                if (height != -1)
                    ERROR("Duplicate height tag.");
                str = val;
                height = get_number(state, &str);
                if (height < 1)
                    ERROR("Invalid height.");
                break;
            case DEFAULT_WIDTH:
                if (fixed_width)
                    ERROR("Fixed-width and default width tags are mutually exclusive.");
                if (default_width != -1)
                    ERROR("Duplicate width tag.");
                str = val;
                default_width = get_number(state, &str);
                if (default_width < 1)
                    ERROR("Invalid default width.");
                break;
#define PROCESS_OPTIONAL_METADATA_ITEM(VARIABLE, MESSAGE) \
				if (VARIABLE != -1) { ERROR(MESSAGE " already set."); } \
				str = val; VARIABLE = get_number(state, &str);
            case ITALIC_ADJUST:
                PROCESS_OPTIONAL_METADATA_ITEM(italic_space_adjust, "Italic adjust");
                break;
            case CAP_HEIGHT:
                PROCESS_OPTIONAL_METADATA_ITEM(cap_height, "Capital height");
                break;
            case X_HEIGHT:
                PROCESS_OPTIONAL_METADATA_ITEM(x_height, "X height");
                break;
            case BASELINE_HEIGHT:
                PROCESS_OPTIONAL_METADATA_ITEM(baseline_height, "Baseline height");
                break;
            case SPACE_ABOVE:
                PROCESS_OPTIONAL_METADATA_ITEM(space_above, "Space above");
                break;
            case SPACE_BELOW:
                PROCESS_OPTIONAL_METADATA_ITEM(space_below, "Space below");
                break;
            case STYLE:
                r = check_string_for_value(val, &styles);
                if (style == -1)
                    style = 0;
                str = val;
                if (r >= 0)
                    style |= r;
                else
                    style |= get_number(state, &str);
                break;
            case WEIGHT:
                if (weight != -1)
                    ERROR("Duplicate weight tag.");
                weight = check_string_for_value(val, &weights);
                if (weight >= 0)
                    break;
                str = val;
                weight = get_number(state, &str);
                break;
            case DOUBLE_WIDTH_DEFAULT:
                r = check_string_for_value(val, &bools);
                if (r < 0)
                    ERROR("Invalid boolean.");
                default_double_width = r;
                break;
            case FIXED_WIDTH:
                if (fixed_width)
                    ERROR("Duplicate fixed-width tag.");
                if (default_width != -1)
                    ERROR("Fixed-width and default width tags are mutually exclusive.");
                str = val;
                fixed_width = true;
                default_width = get_number(state, &str);
                if (default_width < 1)
                    ERROR("Invalid fixed width.");
                break;
            default:
                throw_errorf(internal_error, "Oops, forgot to implement %i\n\n", r);
        }
    } while (true);
    if (height == -1)
        ERROR("Need to set font's height.");
    target->height = (uint8_t)height;
#define SET_OPTIONAL_PARAM(X) if (X >= 0) target->X = (uint8_t)X; else target->X = 0;
    SET_OPTIONAL_PARAM(italic_space_adjust);
    SET_OPTIONAL_PARAM(space_above);
    SET_OPTIONAL_PARAM(space_below);
    SET_OPTIONAL_PARAM(weight);
    SET_OPTIONAL_PARAM(style);
    SET_OPTIONAL_PARAM(cap_height);
    SET_OPTIONAL_PARAM(x_height);
    SET_OPTIONAL_PARAM(baseline_height);
    do {
        double_width = default_double_width;
        /* There's width, default_width, and glyph_width, as well as fixed_width */
        width = -1;
        do {
            r = get_next_line(state);
            if (r == LINE_TOO_LONG)
                if (*eat_whitespace(state->line) == ':')
                    continue;
                else
                    ERROR("Line too long.");
            if (r == EOF && (state_var.line_len == 0 || *eat_whitespace(state_var.line) == '\0'))
                goto file_done;
            r = *eat_whitespace(state->line);
            if (r == ':' || r == '\0')
                continue;
            tag_length = extract_field(state->line, false, tag, MAX_LINE_LENGTH);
            if (tag_length == EOF)
                ERROR("Tag expected.");
            CHECK_FOR_ERROR(tag_length);
            r = extract_field(state->line + tag_length, true, val, MAX_LINE_LENGTH);
            CHECK_FOR_ERROR(r);
            r = check_string_for_value(tag, &glyph_tags);
            if (r < 0)
                continue;
            if (r == DATA)
                break;
            switch (r) {
                case CODEPOINT:
                    str = val;
                    codepoint = get_number(state, &str);
                    if (codepoint > 255)
                        ERROR("Cannot have code point greater than zero.");
                    break;
                case DOUBLE_WIDTH:
                    r = check_string_for_value(val, &bools);
                    if (r < 0)
                        ERROR("Invalid boolean.");
                    double_width = r;
                    break;
                case WIDTH:
                    if (width != -1)
                        ERROR("Duplicate width tag.");
                    str = val;
                    width = get_number(state, &str);
                    if (fixed_width && width != default_width)
                        ERROR("Fixed-width is active; cannot override width.");
                    if (width < 1 || width > 24)
                        ERROR("Invalid width.");
                    break;
            }
        } while (true);
        /* TODO: Validate that we're not reading a duplicate instance of a glyph. */
        if (codepoint > 255)
            ERROR("Too many code points.");
        if (target->bitmaps[codepoint] != NULL)
            ERROR("Duplicate code point definition.");
        glyph_width = 0;
        for (line = 0; line < height; line++) {
            r = get_next_line(state);
            CHECK_FOR_ERROR(r);
            bitmap_line_t bitmap = prase_bitmap(state->line, double_width);
            glyph_data[line] = bitmap.bitmap;
            if (bitmap.width > glyph_width)
                glyph_width = bitmap.width;
        }
        /* Figure out intended width */
        if (fixed_width) {
            if (glyph_width > default_width)
                ERROR("Fixed width is specified, and glyph is too wide.");
            width = default_width;
        } else if (width == -1) {
            if (glyph_width > default_width)
                width = glyph_width;
            else
                width = default_width;
        } else if (glyph_width > width)
            ERROR("Glyph wider than manually set width.");
        int columns = byte_columns(width);
        target->widths_table[codepoint] = width;
        fontlib_bitmap_t *bitmap_data = malloc(sizeof(fontlib_bitmap_t) + height * columns - sizeof(uint8_t));
        if (bitmap_data == NULL)
            throw_error(malloc_failed, "parse_file: Failed to allocate a bitmap.");
        bitmap_data->length = height * columns;
        uint8_t *ptr = bitmap_data->bytes;
        target->bitmaps[codepoint] = bitmap_data;
        /* Order bytes into correct order in bitmap. */
        for (line = 0; line < height; line++) {
            uint32_t pixels = glyph_data[line];
            for (int c = 0; c < columns; c++) {
                *ptr++ = (uint8_t)(pixels >> 24);
                pixels <<= 8;
            }
        }
        if (codepoint < first_glyph)
            first_glyph = codepoint;
        count++;
        codepoint++;
    } while (true);
file_done:
    if (!count)
        throw_error(text_parser_error, "Reached end of file without reading any glyphs.");
    target->first_glyph = (uint8_t)first_glyph;
    codepoint = first_glyph;
    for (int i = count; i > 0; i--)
        if (!target->bitmaps[codepoint++])
            throw_errorf(text_parser_error, "Need definition for glyph %i (0x%02X).", codepoint, codepoint);
    target->total_glyphs = count;
    /* Widths and bitmaps tables need to be trimmed. */
    if (first_glyph) {
        memmove(target->widths_table, target->widths_table + first_glyph, sizeof(target->widths_table[0]) * count);
        memmove(target->bitmaps, target->bitmaps + first_glyph, sizeof(target->bitmaps[0]) * count);
    }
    return target;
}
