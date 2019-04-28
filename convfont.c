#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef _MSC_VER
#include "getopt.h"
#pragma warning(disable : 4996)
#else
#include <getopt.h>
#endif
#include "convfont.h"
#include "parse_fnt.h"
#include "serialize_font.h"

/* http://benoit.papillault.free.fr/c/disc2/exefmt.txt */

#define VERSION_MAJOR 0
#define VERSION_MINOR 92


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

string_value_pair_t weight_names[] = {
    { "thin", 0x20 },
    { "extra light", 0x30 },
    { "extralight", 0x30 },
    { "light", 0x40 },
    { "semilight", 0x60 },
    { "normal", 0x80 },
    { "medium", 0x90 },
    { "semibold", 0xA0 },
    { "bold", 0xC0 },
    { "extra bold", 0xE0 },
    { "extrabold", 0xE0 },
    { "black", 0xF0 }
};

string_list_t weights = {
    .count = 12,.strings = weight_names
};

string_value_pair_t style_names[] = {
    { "sans-serif", 0 },
    { "sansserif", 0 },
    { "serif", 1 },
    { "upright", 0 },
    { "oblique", 2 },
    { "italic", 4 },
    { "monospaced", 8 },
    { "fixed", 8 },
    { "proportional", 0 }
};

string_list_t styles = {
    9, style_names
};

/* Compares a string against a list of strings and numeric values to associate
with that string.  Returns -1 if no match is found. */
int check_string_for_value(char *string, string_list_t *possible_values) { 
    for (int i = 0; i < possible_values->count; i++)
        if (!strcmp(string, possible_values->strings[i].string))
            return possible_values->strings[i].value;
    return -1;
}



/*******************************************************************************
*                                   ERRORS                                     *
*******************************************************************************/

bool unix_newline_style =
#ifdef _WIN32
false
#else
true
#endif
;

int verbosity = 0;

void throw_error(const int code, const char *string) {
    if (string != NULL)
        fprintf(stderr, "ERROR: %s\n", string);
    exit(code);
}



/*******************************************************************************
*                         SOME OUTPUT RELATED STUFF                            *
*******************************************************************************/

void output_format_byte(const uint8_t byte, void *custom_data) {
    fputc(byte, custom_data);
}

typedef struct {
    FILE *file;
    int row_counter;
    bool first_line;
} format_c_array_data_t;

void print_newline(FILE *file) {
    if (!unix_newline_style)
        fputc('\r', file);
    fputc('\n', file);
}

void output_format_c_array(const uint8_t byte, void *custom_data) {
    format_c_array_data_t *state = (format_c_array_data_t *)custom_data;
    if (!state->row_counter)
        if (state->first_line)
            state->first_line = false;
        else {
            fputc(',', state->file);
            print_newline(state->file);
        }
    else
        fprintf(state->file, ", ");
    fprintf(state->file, "0x%02X", byte);
    state->row_counter = (state->row_counter + 1) % 16;
}

void write_string(char *string, FILE *out_file) {
    do
        fputc(*string, out_file);
    while (*string++ != '\0');
}



/*******************************************************************************
*                                    HELP                                      *
*******************************************************************************/

void show_help(char *name) {
    printf("\nUsage:\n"
	"\t%s -o <output format> -f <font FNT> [-metrics] [-f <font FNT 2> [-metrics]] <output file name>\n"
        "\tSpecifying more than one input font is only valid for the font pack output format.\n"
        "\nOutput formats:\n"
        "\t-o fontpack: A fontpack ready to be passed to convhex\n"
        "\t-o carray: A C-style array\n"
        "\t-o asmarray: An assembly-style array\n"
        "\t-o binary: A straight binary blob\n"
#ifdef _WIN32
        "\t-Z: Use CR+LF newlines (default for this platform)\n"
        "\t-z: Use LR newlines instead of CR+LF newlines\n"
#else
        "\t-z: Use LF newlines (default for this platform)\n"
        "\t-Z: Use CR+LF newlines instead of LF newlines\n"
#endif
        "\nIndividual font properties:\n"
        "\t-f: <file name> input Font\n"
        "\t-a: <n> space Above\n"
        "\t-b: <n> space Below\n"
        "\t-i: <n> Italic space adjust\n"
        "\t-w: <n> Weight\n"
        "\t    The following strings may also be used:\n"
        "\t        thin, extralight, light, semilight, normal, medium, semibold, bold, extrabold, black\n"
        "\t-s: <n> Style\n"
        "\t    The following strings may also be used:\n"
        "\t        sans-serif, serif, upright, oblique, italic, monospaced, proportional\n"
        "\t-c: <n> Cap height\n"
        "\t-x: <n> x height\n"
        "\t-l: <n> baseLine height\n"
        "\tNumbers may be prefixed with 0x to specify hexadecimal instead of decimal.\n"
        "\nFont pack properties:\n"
        "\t-N: \"<s>\" font pack Name\n"
        "\t-A: \"<s>\" Author\n"
        "\t-C: \"<s>\" pseudoCopyright\n"
        "\t-D: \"<s>\" Description\n"
        "\t-V: \"<s>\" Version\n"
        "\t-P: \"<s>\" code Page\n", name);
}



/*******************************************************************************
*                                    MAIN                                      *
*******************************************************************************/

#define MAX_FONTS 64

fontlib_font_t *fonts[MAX_FONTS] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};
int fonts_loaded = 0;

int main(int argc, char *argv[]) {
    FILE *in_file;
    FILE *out_file;

    printf("convfont v%u.%u by drdnar\n", VERSION_MAJOR, VERSION_MINOR);

    if (argc <= 1) {
        printf("No inputs supplied.\n\n");
        show_help(argv[0]);
        return 0;
    }

    /* Settings */
    char *font_pack_name = NULL;
    char *author = NULL;
    char *pseudocopyright = NULL;
    char *description = NULL;
    char *version = NULL;
    char *codepage = NULL;
    char *output_file_name = NULL;
    char *recent_input_file_name = NULL;
    fontlib_font_t *current_font = NULL;
    int temp_n;
    output_formats_t output_format = output_unspecified;

    int option;

    while ((option = getopt(argc, argv, "hvo:Zf:a:b:i:w:s:c:x:l:N:A:C:D:V:P:")) != -1) {
        switch (option) {
            case 'h':
                show_help(argv[0]);
                return 0;
            case 'v':
                verbosity++;
                break;
            case 'o':
                if (output_format != output_unspecified)
                    throw_error(bad_options, "-o: Cannot specify more than one output format.");
                switch (optarg[0]) {
                    case 'c':
                        output_format = output_c_array;
                        break;
                    case 'a':
                        output_format = output_asm_array;
                        break;
                    case 'f':
                    case 'p':
                        output_format = output_fontpack;
                        break;
                    case 'b':
                        output_format = output_binary_blob;
                        break;
                    default:
                        throw_error(bad_options, "-o: Unknown output format.");
                        break;
                }
                break;
            case 'Z':
                unix_newline_style = false;
                break;
            case 'z':
                unix_newline_style = true;
                break;
            case 'f':
                if (fonts_loaded > 0 && output_format != output_fontpack)
                    throw_error(bad_options, "-f: Cannot have multiple input fonts unless -o fontpack is specified first.");
                if (fonts_loaded >= MAX_FONTS - 1)
                    throw_error(bad_options, "-f: Too many fonts.  What on Earth makes you think your font pack needs so many fonts?");
                recent_input_file_name = optarg;
                if (verbosity >= 1)
                    printf("Processing input file %s . . .\n", recent_input_file_name);
                in_file = fopen(recent_input_file_name, "rb");
                if (!in_file)
                    throw_error(bad_infile, "-f: Cannot open input file.");
                int ver = read_word(in_file);
                if (ver != 0x200 && ver != 0x300)
                    throw_error(bad_infile, "-f: Input file does not appear to be an FNT at all.");
                current_font = parse_fnt(in_file, 0);
                fclose(in_file);
                fonts[fonts_loaded++] = current_font;
                break;
            case 'a':
                if (current_font == NULL)
                    throw_error(bad_options, "-a: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > 64 || temp_n < 0)
                    throw_error(bad_options, "-a: Number too large or small.");
                current_font->space_above = (uint8_t)temp_n;
                break;
            case 'b':
                if (current_font == NULL)
                    throw_error(bad_options, "-b: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > 64 || temp_n < 0)
                    throw_error(bad_options, "-b: Number too large or small.");
                current_font->space_below = (uint8_t)temp_n;
                break;
            case 'i':
                if (current_font == NULL)
                    throw_error(bad_options, "-i: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > 24 || temp_n < 0)
                    throw_error(bad_options, "-i: Number too large or small.");
                current_font->italic_space_adjust = (uint8_t)temp_n;
                break;
            case 'w':
                if (current_font == NULL)
                    throw_error(bad_options, "-w: Must specify a font before specifying metrics.");
                temp_n = check_string_for_value(optarg, &weights);
                if (temp_n == -1) {
                    temp_n = (int)strtol(optarg, NULL, 0);
                    if (temp_n > 255 || temp_n < 0)
                        throw_error(bad_options, "-w: Number too large or small.");
                }
                current_font->weight = (uint8_t)temp_n;
                break;
            case 's':
                if (current_font == NULL)
                    throw_error(bad_options, "-s: Must specify a font before specifying metrics.");
                temp_n = check_string_for_value(optarg, &styles);
                if (temp_n == -1) {
                    temp_n = (int)strtol(optarg, NULL, 0);
                    if (temp_n > 255 || temp_n < 0)
                        throw_error(bad_options, "-s: Number too large or small.");
                }
                current_font->style |= (uint8_t)temp_n;
                break;
            case 'c':
                if (current_font == NULL)
                    throw_error(bad_options, "-c: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > current_font->height || temp_n < 0)
                    throw_error(bad_options, "-c: Number too large or small.");
                current_font->cap_height = (uint8_t)temp_n;
                break;
            case 'x':
                if (current_font == NULL)
                    throw_error(bad_options, "-x: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > current_font->height || temp_n < 0)
                    throw_error(bad_options, "-x: Number too large or small.");
                current_font->x_height = (uint8_t)temp_n;
                break;
            case 'l':
                if (current_font == NULL)
                    throw_error(bad_options, "-l: Must specify a font before specifying metrics.");
                temp_n = (int)strtol(optarg, NULL, 0);
                if (temp_n > current_font->height || temp_n < 0)
                    throw_error(bad_options, "-l: Number too large or small.");
                current_font->baseline_height = (uint8_t)temp_n;
                break;
            case 'N':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-N: Must specify font pack output format.");
                if (font_pack_name != NULL)
                    throw_error(bad_options, "-N: Duplicate.");
                if (strlen(optarg) > 255)
                    printf("-N: Recommend against such a long string.\n");
                font_pack_name = optarg;
                break;
            case 'A':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-A: Must specify font pack output format.");
                if (author != NULL)
                    throw_error(bad_options, "-A: Duplicate.");
                if (strlen(optarg) > 255)
                    printf("-A: Recommend against such a long string.  You are not an aristocrat.\n");
                author = optarg;
                break; 
            case 'C':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-C: Must specify font pack output format.");
                if (pseudocopyright != NULL)
                    throw_error(bad_options, "-C: Duplicate.");
                if (strlen(optarg) > 255)
                    throw_error(bad_options, "-C: Screw the copyright lawyers.  You don't need such a long copyright string.\n");
                pseudocopyright = optarg;
                break;
            case 'D':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-D: Must specify font pack output format.");
                if (description != NULL)
                    throw_error(bad_options, "-D: Duplicate.");
                if (strlen(optarg) > 255)
                    printf("-D: Recommend against such a long string.  (It's called the \"description\" field, not \"dissertation\"!)\n");
                description = optarg;
                break;
            case 'V':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-V: Must specify font pack output format.");
                if (version != NULL)
                    throw_error(bad_options, "-V: Duplicate.");
                if (strlen(optarg) > 255)
                    printf("-V: Recommend against such a long string.  (It's called the version field, not the changelog!)\n");
                version = optarg;
                break;
            case 'P':
                if (output_format != output_fontpack)
                    throw_error(bad_options, "-P: Must specify font pack output format.");
                if (codepage != NULL)
                    throw_error(bad_options, "-P: Duplicate.");
                if (strlen(optarg) > 255)
                    printf("-P: Strongly recommend against such a long string.  (What, are you trying to embed a complete Unicode translation table?)\n");
                codepage = optarg;
                break;
            case '?':
                throw_error(bad_options, "Unknown option; check syntax.");
                break;
        }
    }

    if (optind == argc)
        throw_error(bad_options, "Last parameter must be output file name; none was given.");
    output_file_name = argv[optind];
    if (optind < argc - 1)
        throw_error(bad_options, "Too many trailing parameters.");
    if (current_font == NULL)
        throw_error(bad_options, "No input font(s) given. . . . Nothing to do.");

    /* Now write output */
    format_c_array_data_t c_array_data;
    out_file = fopen(output_file_name, "wb");
    if (!out_file)
        throw_error(bad_outfile, "Cannot open output file.");
    if (output_format == output_fontpack) {
        /* Write header */
        for (char *s = "FONTPACK"; *s != '\0'; s++)
            fputc(*s, out_file);
        /* Offset to metadata */
        int location = 12 + fonts_loaded * 3;
        int mdlocation = 0;
        bool no_metadata = font_pack_name == NULL && author == NULL && pseudocopyright == NULL && description == NULL && version == NULL && codepage == NULL;
        if (no_metadata)
            output_ezword(0, output_format_byte, out_file);
        else {
            output_ezword(location, output_format_byte, out_file);
            location += MEATADATA_STRUCT_SIZE;
            mdlocation = location;
            if (font_pack_name != NULL)
                location += (int)strlen(font_pack_name) + 1;
            if (author != NULL)
                location += (int)strlen(author) + 1;
            if (pseudocopyright != NULL)
                location += (int)strlen(pseudocopyright) + 1;
            if (description != NULL)
                location += (int)strlen(description) + 1;
            if (version != NULL)
                location += (int)strlen(version) + 1;
            if (codepage != NULL)
                location += (int)strlen(codepage) + 1;
        }
        /* Font count */
        fputc(fonts_loaded, out_file);
        /* Fonts table */
        for (int i = 0; i < fonts_loaded; location += compute_font_size(fonts[i++]))
            output_ezword(location, output_format_byte, out_file);
        if (location >= MAX_APPVAR_SIZE) {
            fclose(out_file);
            remove(argv[optind]);
            throw_error(bad_options, "Cannot form appvar; output appvar size would exceed 64 K appvar size limit.");
        }
        /* Serialize font metadata */
        if (!no_metadata) {
            output_ezword(MEATADATA_STRUCT_SIZE, output_format_byte, out_file);
            if (font_pack_name != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(font_pack_name) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (author != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(author) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (pseudocopyright != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(pseudocopyright) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (description != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(description) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (version != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(version) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (codepage != NULL) {
                output_ezword(mdlocation, output_format_byte, out_file);
                mdlocation += (int)strlen(codepage) + 1;
            } else
                output_ezword(0, output_format_byte, out_file);
            if (font_pack_name != NULL)
                write_string(font_pack_name, out_file);
            if (author != NULL)
                write_string(author, out_file);
            if (pseudocopyright != NULL)
                write_string(pseudocopyright, out_file);
            if (description != NULL)
                write_string(description, out_file);
            if (version != NULL)
                write_string(version, out_file);
            if (codepage != NULL)
                write_string(codepage, out_file);
        }
        for (int i = 0; i < fonts_loaded; i++) {
            current_font = fonts[i];
            serialize_font(current_font, output_format_byte, out_file);
            fonts[i] = NULL;
            free_fnt(current_font);
        }
        current_font = NULL;
    } else {
        switch (output_format) {
            case output_c_array:
                c_array_data.file = out_file;
                c_array_data.row_counter = 0;
                c_array_data.first_line = true;
                serialize_font(current_font, output_format_c_array, &c_array_data);
                break;
            case output_asm_array:
                fprintf(out_file, ".header:"); print_newline(out_file);
                fprintf(out_file, "\tdb\t0 ; font format version"); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; height", current_font->height); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; glyph count", current_font->total_glyphs & 0xFF); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; first glyph", current_font->first_glyph); print_newline(out_file);
                fprintf(out_file, "\tdl\t.widthsTable - .header ; offset to widths table"); print_newline(out_file);
                fprintf(out_file, "\tdl\t.bitmapsTable - .header ; offset to bitmaps offsets table"); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; italics space adjust", current_font->italic_space_adjust); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; suggested blank space above", current_font->space_above); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; suggested blank space below", current_font->space_below); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; weight (boldness/thinness)", current_font->weight); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; style field", current_font->style); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; capital height", current_font->cap_height); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; lowercase x height", current_font->x_height); print_newline(out_file);
                fprintf(out_file, "\tdb\t%i ; baseline height", current_font->baseline_height); print_newline(out_file);
                fprintf(out_file, ".widthsTable: ; start of widths table"); print_newline(out_file);
                for (int i = 0; i < current_font->total_glyphs; i++) {
                    fprintf(out_file, "\tdb\t%i ; Code point $%02X %c", current_font->widths_table[i], i + current_font->first_glyph, i + current_font->first_glyph);
                    print_newline(out_file);
                }
                fprintf(out_file, ".bitmapsTable: ; start of table of offsets to bitmaps"); print_newline(out_file);
                for (int i = 0; i < current_font->total_glyphs; i++) {
                    int width = current_font->widths_table[i];
                    fprintf(out_file, "\tdw\t.glyph_%02X - .header", i + current_font->first_glyph);
                    if (width <= 16)
                        fprintf(out_file, " - %i", 3 - byte_columns(width));
                    fprintf(out_file, "; %c", i + current_font->first_glyph);
                    print_newline(out_file);
                }
                for (int i = 0; i < current_font->total_glyphs; i++) {
                    int width = current_font->widths_table[i];
                    int bwidth = byte_columns(width);
                    fprintf(out_file, ".glyph_%02X: ; %c", i + current_font->first_glyph, i + current_font->first_glyph); print_newline(out_file);
                    int byte = 0;
                    for (int row = 0; row < current_font->height; row++) {
                        switch (bwidth) {
                            case 1:
                                fprintf(out_file, "\tdb\t");
                                break;
                            case 2:
                                fprintf(out_file, "\tdw\t");
                                break;
                            case 3:
                                fprintf(out_file, "\tdl\t");
                                break;
                            default:
                                throw_error(internal_error, "byte_columns failed to give 1, 2, or 3.  That should not happen.");
                                break;
                        }
                        /* The format requires omitting the least-significant byte(s) if they're unused. */
                        for (int col = 0; col < bwidth; col++) {
                            int b = current_font->bitmaps[i]->bytes[byte++];
                            for (int bit = 0; bit < 8; bit++, b <<= 1)
                                if (b & 0x80)
                                    fprintf(out_file, "1");
                                else
                                    fprintf(out_file, "0");
                        }
                        fprintf(out_file, "b");
                        print_newline(out_file);
                    }
                }
                break;
            case output_binary_blob:
                serialize_font(current_font, output_format_byte, out_file);
                break;
            case output_unspecified:
                throw_error(internal_error, "-o: No output format specified.");
                break;
            default:
                throw_error(internal_error, "-o: Someone attempted to add a new output format without actually coding it.");
                break;
        }
        free_fnt(current_font);
        current_font = NULL;
        fonts[0] = NULL;
    }
    printf("Output size: %li bytes; conversion finished.\n", ftell(out_file));
    fclose(out_file);

    return 0;
}
