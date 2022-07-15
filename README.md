# convfont
`convfont` converts Windows .FNT files or textual font files into a form usable by FontLibC.

Usage:

`convfont -o <output format> -f <font FNT> [-metrics] [-t <textual font 2> [-metrics]] <output file name>`

Use `-t` instead of `-f` if using the text-based font format.

The `fontpack` output format supports packing multiple fonts.
To specify multiple fonts, use `-f <font>` (or `-t <font>`) repeatedly for each input font.

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
You may directly specify a numeric value, optionally using the prefix `0x` for hexadecimal; 
see `fontlibc.h` for standard values.
Alternatively, the following names may be used: `thin`, `"extra light"`, `extralight` (synonym for previous), `light`, `semilight`, `normal`, `semibold`, `bold`, `"extra bold"`, `extrabold` (synonym for previous), `black`.

### Style
`-s` specifies the style of the font.
This is purely used for font selection and has no direct effect on how the font is displayed.
You may directly specify a numeric value, optionally using the prefix `0x` for hexadecimal; 
see `fontlibc.h` for standard values.
Alternatively, the following names may be used: `sans-serif`, `sansserif` (synonym for previous), `serif`, `upright`, `oblique`, `italic`, `monospaced`, `fixed` (synonym for previous), `proportional`.

If `-s` is specified more than once for the same font, the values are ORed together. For example, you can specify a font as both italic and serif font by doing `-s serif -s italic`.

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
font packs may also contain metadata describing them.
Metadata strings should be terse, as they consume limited memory on-calculator.

The version field is a string, not a number.
It is not intended to support automatic version control.

Note that the United States does not allow copyrighting bitmapped fonts, though many other jurisdictions do.

## Text-Based Font Format
`convfont`'s original input format was the legacy Windows `.fnt` format.
However, there are not a lot of tools for creating `.fnt` files.
`convfont` now also supports a purely text-based input format, which can be created and edited using any text editor.

### Encoding
`convfont` will automatically detect UTF-8 and UTF-16 (BE/LE) file encodings.
Do not use combining diacritics; they will be parsed as separate characters.

Standard line endings of Unix/Linux (LF), Mac (CR), and Windows (CR+LF) are all supported.

### File Format
A textual font file starts with a single line of text containing only the text `convfont`.
This signature identifies the file as being a valid input file, as well as allowing for automatic detection of encoding.

Every line after the signature will either be a metadata line or a bitmap line.
A metadata line specifies information such as a font's height or a glyph's code point.
A bitmap lines encodes a line of pixels as ASCII art.

A metadata line has three parts: a tag which identifies what the line describes,
a colon `:` that separates the tag from the data, and the data itself.
Tags are not case sensitive, but the data specified may be.
Whitespace before and after a tag and a value is ignored.

A line starting with a single colon is treated as a comment.
Additionally, when a metadata line is expected, a completely blank line will be ignored.
(Blank lines are not ingored in bitmaps.)

### Numbers
`convfont` allows specifying numbers mostly the same way as C.

- Negative values are not possible.
- A simple number is automatically in decimal.
- Unlike in C, prefixing a number with a leading zero *does not* make it octal. (See below.)
- A prefix of `0x` or `$` will cause a number to be interpreted as hexadecimal.
- A quote mark `'` may be used to specify a character literal.  For example, `'A'` is the same as decimal 65.
- Most C escape sequences may be used in character literals.  For example, `'\n'` is newline or decimal 10.
- Octal and hexadecimal escape sequences may also be used.  For example, `'\14'` is the octal number 14 (12 decimal).

To specify the code point for `A`, you could either write `Code point: 65` or `Code point: 'A'`.

### Font Metadata
The metadata after the signature specifies information about the entire font.
A font must specify its height here.
Additionally, metrics such as weight and cap height may be specified here.
The font's metadata block is terminated with a `Font data` tag.

The following tags are recognized:
- `Height` (required)
- `Double width` (described below)
- `Default width` (described below)
- `Fixed-width` (described below)
- `Font data` (ends font metadata block)
- `Weight` (optional metrics)
- `Style`
- `Italic adjust`
- `Cap height`
- `x-height`
- `Baseline`
- `Space above`
- `Space below`

Characters in most monospaced fonts are about twice as tall as they are wide, so a `double width` tag is supported.
If set to `true`, two characters from the input file are read for each pixel, halving the width.
This allows the ASCII art to be somewhat less distorted.

If a `default width` is specified, 
any glyph without an explicit `width` tag will be expanded to be at least as wide as the specified `default width`.

For a fixed-width font, the `fixed-width` tag may be specified.
It sets the width of all glyphs in the font.
If a bitmap is narrower than this, it is automatically padded with blank pixels to the right.
If a bitmap is wider, it is treated as an error.

The optional metrics allow specifying those values in the file, instead of on `convfont`'s command line.
If metrics are specified on the command line, they will override the values in the file.

### Glyph Data
After the font's metadata block comes a font's glyphs.
Each glyph has its own metadata block, followed by a bitmap block.

The following tags are recognized:
- `Double width`
- `Code point`
- `Width`
- `Data` (required to start a glyph's bitmap)

The `double width` tag will override for a single glyph any `double width` specified in the font's metadata block.
`Width` forces the width of a single glyph to be a given value.

`Code point` specifies which code point a glyph should be in.
If none is specified for the first glyph in a file, it defaults to zero.
If none is specified for a subsequent glyph, it defaults to one more than the previous.

A bitmap line consists of an ASCII-art drawing of a glyph's pixels.
Whitespace is not ignored and comments are not possible.
Spaces are used for blank pixels, and almost anything else is treated as a set pixel.

Em dash (U+2014), en dash (U+2013), figure dash (U+2012), minus sign (U+2212),
and `0` (U+0030) are also treated as blank pixels.
The figure dash is normally the same width as the digits 0-9.
The minus sign is normally the same width as a plus (+) sign.
These provide some additional flexibility in the event that your text editor does not allow selecting a monospaced font.

### Example
 ```
 convfont
 Double width: true
 Height: 8
 Fixed width: 6
 Font data:
 Codepoint: 'A'
 Data:
   ******
 **      **
 **      **
 **********
 **      **
 **      **
 **      **
 
 Data:
 ########
 ##      ##
 ##      ##
 ########
 ##      ##
 ##      ##
 ########
 
 :Usings ones and zeros.
 Data:
 001111110000
 110000001100
 110000000000
 110000000000
 110000000000
 110000001100
 001111110000
 000000000000
 ```
