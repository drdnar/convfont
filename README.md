# convfont
`convfont` converts Windows .FNT files into a form usable by FontLibC.

Usage:

`convfont -o <output format> -f <font FNT> [-metrics] [-f <font FNT 2> [-metrics]] <output file name>`

## Font Properties



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
