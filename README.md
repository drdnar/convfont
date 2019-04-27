# convfont
`convfont` converts Windows .FNT files into a form usable by FontLibC.

Usage:

`convfont -o <output format> -f <font FNT> [-metrics] [-f <font FNT 2> [-metrics]] <output file name>`

The `fontpack` output format supports packing multiple fonts.
To specify multiple fonts, use `-f <font>` repeatedly for reach input font.

## Font properties
Font properties are metadata associated with each font that assist in layout and font selection.

All font properties default to zero if left unspecified.

### Space above and below
Adding additional blank space between lines of text helps improve readability.
FontLibC can add such blank space automatically based on metadata in the font or programmer request.
Specifying blank space does not change the byte size of the font, only its layout.

`-a` specifies recommended additional blank space above each line of text.

`-b` specifies recommended additional blank space below.

### Italic space adjust
`-i` is for italic space adjust, a feature allowing the left side of a glyph to partially overlap the right of the previous glyph.

### Weight
`-w` specifies the weight of the font.
This is purely used for font selection and has no direct effect on how the font is displayed.
See `fontlibc.h` for standard values.

### Style
`-s` specifies the style of the font.
This is purely used for font selection and has no direct effect on how the font is displayed.
See `fontlibc.h` for standard values.

### Cap, x, and baseline height
`-c`, `-x`, and `-l` specify these values.
They count lines from the top of the glyph going down.
Client layout code can use these values to align text from different font families.


## Output formats
`convfont` supports several different output formats, allowing you to select a format suitable for your needs.

### C Array
The `carray` output format allows embedding a font directly into a C program.
For example, if you specify an output file named `myfont.inc` you can use it in your program like so:
 ```
 unsigned char my_font_data[] = {
     #include "myfont.inc"
 }
 fontlib_font_t my_font* = (fontlib_font_t *)my_font_data;
 . . .
 fontlib_SetFont(my_font, 0);
 ```

### Assembly Array
`asmarray` is like `carray` except for use in assembly programs.

### Binary Blob
`binary` produces a binary blob you can post-process for whatever other purpose you might need.

### Font Pack
A `fontpack` consists of multiple fonts, typically composing a single typeface.
Font packs allow multiple programs to use the same font data, saving space on-calculator.
Since font packs can be archived, they also free up user RAM.

You can bundle multiple sizes of a font together.
When your calculator program runs, you can use a routine like `fontlib_GetFontByStyle()` to get the font you want from your font pack.

**A `fontpack` binary needs to be packaged into an appvar before it can be used on-calculator:**

```convhex -a -v myfont.bin myfont.8xv```

#### Font Pack Metadata
Because font packs are intended to represent a single typeface and allowing users to select a font is an anticipated use case,
font packs may also contain metadata describing them. Metadata strings should be terse, as they consume limited memory on-calculator.

Note that the United States does not allow copyrighting bitmapped fonts, though many other jurisdictions do.
