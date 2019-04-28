#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "convfont.h"
#include "parse_fnt.h"

uint8_t read_byte(FILE *input) {
    int c = fgetc(input);
    if (c == EOF)
        throw_error(invalid_fnt, "Out of BYTEs in input.");
    return (uint8_t)c;
}

int16_t read_word(FILE *input) {
    int a, b = 0;
    for (int i = 0; i < 2; i++) {
        a = fgetc(input);
        if (a == EOF)
            throw_error(invalid_fnt, "Out of WORDs in input.");
        b |= a << (8 * i);
    }
    return (int16_t)b;
}

int32_t read_dword(FILE *input) {
    int a, b = 0;
    for (int i = 0; i < 4; i++) {
        a = fgetc(input);
        if (a == EOF)
            throw_error(invalid_fnt, "Out of DWORDs in input.");
        b |= a << (8 * i);
    }
    return (int16_t)b;
}

/* Frees a malloc()ed font for you.
 * @param font Pointer to the font to free. */
void free_fnt(fontlib_font_t *font) {
    for (int i = 0; i < font->total_glyphs; i++)
        free(font->bitmaps[i]);
    free(font->bitmaps);
    free(font->widths_table);
    free(font);
}

/* Unpacks an FNT into RAM.
 * @param input The already-opened file to read from.
 * @param offset The location of the FNT in the input file
 * @return A pointer to a malloc()ed font. */
fontlib_font_t *parse_fnt(FILE *input, int offset) {
    /* Part of the idea of reading bytewise instead of trying to load the whole struct at once is to prevent
       portability issues with unaligned reads. */
       /* Allocate font struct */
    fontlib_font_t *target = malloc(sizeof(fontlib_font_t));
    if (!target)
        throw_error(malloc_failed, "parse_fnt: failed to malloc fontlib_font_t");
    /* For locations validation */
    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    /* Ensure we're at the right place */
    fseek(input, offset, SEEK_SET);
    /* Version check, also checks that this even looks like a valid FNT */
    int16_t dfVersion = read_word(input);
    if (verbosity >= 1) printf("dfVersion: 0x%04X\n", dfVersion);
    if (dfVersion != 0x200 && dfVersion != 0x300)
        throw_error(invalid_fnt, "Invalid version field.");
    /* We can validate all offsets against this field. */
    int32_t dfSize = read_dword(input);
    if (verbosity >= 2) printf("dfSize: %i bytes\n", dfSize); 
    if (offset + dfSize > file_size)
        throw_error(invalid_fnt, "dfSize extends past end of file.");
    /* So the copyright field gets a special, fixed location up front, but not the name. . . . */
    const int dfCopyrightSize = 60;
    char dfCopyright[61];
    dfCopyright[60] = 0;
    for (int i = 0; i < dfCopyrightSize; i++)
        dfCopyright[i] = read_byte(input);
    if (verbosity >= 1) printf("dfCopyright: %s\n", dfCopyright);
    /* Is this a bitmap or vector font? */
    int16_t dfType = read_word(input);
    if (verbosity >= 2) printf("dfType: 0x%04X\n", dfType);
    bool isRaster = (dfType & 1) == 0;
    if (!isRaster)
        throw_error(invalid_fnt, "Not a raster font.");
    /* Now get a bunch of font metadata */
    int16_t dfPoints = read_word(input);
    if (verbosity >= 1) printf("dfPoints: 0x%04X\n", dfPoints);
    int16_t dfVertRes = read_word(input);
    if (verbosity >= 2) printf("dfVertRes: 0x%04X\n", dfVertRes);
    int16_t dfHorizRes = read_word(input);
    if (verbosity >= 2) printf("dfHorizRes: 0x%04X\n", dfHorizRes);
    int16_t dfAscent = read_word(input);
    if (verbosity >= 1) printf("dfAscent: 0x%04X\n", dfAscent);
    int16_t dfInternalLeading = read_word(input);
    if (verbosity >= 2) printf("dfInternalLeading: 0x%04X\n", dfInternalLeading);
    int16_t dfExternalLeading = read_word(input);
    if (verbosity >= 2) printf("dfExternalLeading: 0x%04X\n", dfExternalLeading);
    uint8_t dfItalic = read_byte(input);
    if (verbosity >= 1) printf("dfItalic: 0x%02X\n", dfItalic);
    uint8_t dfUnderline = read_byte(input);
    if (verbosity >= 2) printf("dfUnderline: 0x%02X\n", dfUnderline);
    uint8_t dfStrikeOut = read_byte(input);
    if (verbosity >= 2) printf("dfStrikeOut: 0x%02X\n", dfStrikeOut);
    int16_t dfWeight = read_word(input);
    if (verbosity >= 1) printf("dfWeight: %i\n", dfWeight);
    uint8_t dfCharSet = read_byte(input);
    if (verbosity >= 1) printf("dfCharSet: 0x%02X\n", dfCharSet);
    int16_t dfPixWidth = read_word(input);
    if (verbosity >= 2) printf("dfPixWidth: 0x%04X\n", dfPixWidth);
    int16_t dfPixHeight = read_word(input);
    if (verbosity >= 2) printf("dfPixHeight: 0x%04X\n", dfPixHeight);
    uint8_t dfPitchAndFamily = read_byte(input);
    if (verbosity >= 2) printf("dfPitchAndFamily: 0x%02X\n", dfPitchAndFamily);
    int16_t dfAvgWidth = read_word(input);
    if (verbosity >= 2) printf("dfAvgWidth: 0x%04X\n", dfAvgWidth);
    int16_t dfMaxWidth = read_word(input);
    if (verbosity >= 1) printf("dfMaxWidth: 0x%04X\n", dfMaxWidth);
    uint8_t dfFirstChar = read_byte(input);
    if (verbosity >= 1) printf("dfFirstChar: 0x%02X\n", dfFirstChar);
    uint8_t dfLastChar = read_byte(input);
    if (verbosity >= 1) printf("dfLastChar: 0x%02X\n", dfLastChar);
    uint8_t dfDefaultChar = read_byte(input);
    if (verbosity >= 2) printf("dfDefaultChar: 0x%02X\n", dfDefaultChar);
    uint8_t dfBreakChar = read_byte(input);
    if (verbosity >= 2) printf("dfBreakChar: 0x%02X\n", dfBreakChar);
    int16_t dfWidthBytes = read_word(input);
    if (verbosity >= 2) printf("dfWidthBytes: 0x%04X\n", dfWidthBytes);
    int32_t dfDevice = read_dword(input);
    if (verbosity >= 2) {
        if (dfDevice != 0) {
            long old_loc = ftell(input);
            fseek(input, offset + dfDevice, SEEK_SET);
            printf("dfDevice: ");
            if (verbosity >= 3) printf("@ 0x%08X ", dfDevice);
            for (char read_char = read_byte(input); read_char != '\0'; read_char = read_byte(input))
                printf("%c", read_char);
            printf("\n");
            fseek(input, old_loc, SEEK_SET);
        }
        else
            printf("dfDevice is not present.\n");
    }
    int32_t dfFace = read_dword(input);
    if (verbosity >= 1) {
        if (dfFace != 0) {
            long old_loc = ftell(input);
            fseek(input, offset + dfFace, SEEK_SET);
            printf("dfFace: ");
            if (verbosity >= 3) printf("@ 0x%08X ", dfFace);
            for (char read_char = read_byte(input); read_char != '\0'; read_char = read_byte(input))
                printf("%c", read_char);
            printf("\n");
            fseek(input, old_loc, SEEK_SET);
        }
        else
            printf("dfFace is not present.\n");
    }
    //DWORD  dfReserved; 
    // more stuff
    int32_t dfBitsPointer = read_dword(input);
    if (verbosity >= 3) printf("dfBitsPointer: 0x%08X\n", dfBitsPointer);
    int32_t dfBitsOffset = read_dword(input);
    if (verbosity >= 3) printf("dfBitsOffset: 0x%08X\n", dfBitsOffset);
    uint8_t dfReserved = read_byte(input);
    if (verbosity >= 2) printf("dfReserved: 0x%02X\n", dfReserved);
    /* Version 3.0 stuff */
    int32_t dfFlags;
    int16_t dfAspace;
    int16_t dfBspace;
    int16_t dfCspace;
    int16_t dfColorPointer;
    int32_t dfReserved1;
    int32_t dfCharTableOffsets[256];
    int16_t dfCharTableWidths[256];

    if (dfVersion == 0x300) {
        dfFlags = read_dword(input);
        if (verbosity >= 2) printf("dfFlags: 0x%08X\n", dfFlags);
        if ((dfFlags & 0x0C) != 0)
            throw_error(invalid_fnt, "DEF_ABC* is not supported.");
        if ((dfFlags & 0xF0) != 0x10)
            throw_error(invalid_fnt, "DFF_1COLOR is required. Seriously, I'm not writing a grayscale rendering library.");
        dfAspace = read_word(input);
        if (verbosity >= 2) printf("dfAspace: 0x%04X\n", dfAspace);
        dfBspace = read_word(input);
        if (verbosity >= 2) printf("dfBspace: 0x%04X\n", dfBspace);
        dfCspace = read_word(input);
        if (verbosity >= 2) printf("dfCspace: 0x%04X\n", dfCspace);
        dfColorPointer = read_word(input);
        if (verbosity >= 3) printf("dfColorPointer: 0x%04X\n", dfColorPointer);
        dfReserved1 = read_word(input);
        if (verbosity >= 4) printf("dfReserved1: 0x%04X\n", dfReserved1);
        /* I have no idea what these bytes might be. */
        read_word(input);
        read_word(input);
        read_word(input);
        read_word(input);
        read_word(input);
    }

    int totalGlyphs = dfLastChar - dfFirstChar + 1;

    if (verbosity >= 2) printf("Reading glyph size and location table . . .\n");
    if (dfVersion == 0x200)
        for (int i = 0; i < totalGlyphs; i++) {
            dfCharTableWidths[i] = read_word(input);
            dfCharTableOffsets[i] = read_word(input);
            if (verbosity >= 3) printf("\tGlyph: 0x%02X width: %i @ 0x%04X\n", i, dfCharTableWidths[i], dfCharTableOffsets[i]);
            if (dfCharTableOffsets[i] > dfSize)
                throw_error(invalid_fnt, "Glyph bitmap location offset is past declared end of FNT struct.");
        }
    else {
        /* Dummy entry needed for some crazy reason */
        int dummy1 = read_word(input);
        int dummy2 = read_dword(input);
        if (verbosity >= 3) printf("\tDummy entry: width: %i @ 0x%08X\n", dummy1, dummy2);
        for (int i = 0; i < totalGlyphs; i++) {
            dfCharTableWidths[i] = read_word(input);
            dfCharTableOffsets[i] = read_dword(input);
            if (verbosity >= 3) printf("\tGlyph: 0x%02X width: %i @ 0x%08X\n", i, dfCharTableWidths[i], dfCharTableOffsets[i]);
            if (dfCharTableOffsets[i] > dfSize)
                throw_error(invalid_fnt, "Glyph bitmap location offset is past declared end of FNT struct.");
        }
    }

    target->fontVersion = 0;
    target->height = (uint8_t)dfPixHeight;
    if (dfLastChar < dfFirstChar)
        throw_error(invalid_fnt, "Negative bitmaps present! (dfFirstChar > dfLastChar)");
    target->total_glyphs = (uint16_t)(dfLastChar - dfFirstChar + 1);
    target->first_glyph = dfFirstChar;
    target->widths_table = calloc(totalGlyphs, sizeof(uint8_t));
    if (!target->widths_table)
        throw_error(malloc_failed, "parse_fnt: failed to calloc widths_table");
    target->bitmaps = calloc(totalGlyphs, sizeof(fontlib_bitmap_t*));
    if (!target->bitmaps)
        throw_error(malloc_failed, "parse_fnt: failed to calloc bitmaps table");
    target->italic_space_adjust = 0;
    target->space_above = 0;
    target->space_below = 0;
    target->weight = 0;
    target->style = 0;
    target->cap_height = 0;
    target->x_height = 0;
    target->baseline_height = (uint8_t)dfAscent;

    if (verbosity >= 2) printf("Parsing glyphs . . .\n");
    for (int i = 0; i < totalGlyphs; i++) {
        if (verbosity >= 4) printf("\tGlyph: 0x%02X data: ", i);
        if (dfCharTableWidths[i] > 24)
            throw_error(invalid_fnt, "Glyph widths greater than 24 are not supported.");
        if (dfCharTableWidths[i] == 0)
            throw_error(invalid_fnt, "Zero-width glyph is a bad idea.");
        target->widths_table[i] = (uint8_t)dfCharTableWidths[i];
        if (fseek(input, offset + dfCharTableOffsets[i], SEEK_SET))
            throw_error(invalid_fnt, "fseek() failed.");
        int columns = byte_columns(dfCharTableWidths[i]);
        fontlib_bitmap_t *bitmap = malloc(sizeof(fontlib_bitmap_t) + target->height * columns - sizeof(uint8_t));
        if (!bitmap)
            throw_error(malloc_failed, "parse_fnt: failed to malloc bitmap");
        bitmap->length = target->height * columns;
        /* Basically, we're just going to transform this from column-major order
            to row-major order. */
        for (int c = 0; c < columns; c++) /* Not a secret message */
            for (int y = 0; y < target->height; y++) {
                bitmap->bytes[y * columns + c] = read_byte(input);
                if (verbosity >= 4) printf("%02X ", bitmap->bytes[y * columns + c]);
            }
        if (verbosity >= 4)
            printf("\n");
        target->bitmaps[i] = bitmap;
    }
    if (verbosity >= 1) printf("Finished processing FNT.\n\n");
    return target;
}
