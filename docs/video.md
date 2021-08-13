# Asspull IIIx Programmers Guide -- Video

The *Asspull IIIx* supports four different screen modes:

1. Text
2. 16-color bitmap
3. 256-color bitmap
4. Tilemap

## Text mode

In text mode, each cell is stored as a 16-bit value. Technically speaking it's the exact same format as in CGA-
compatible text mode, but with a few more lines in total.

    BBBB FFFF CCCC CCCC
    |    |    |__________ Character
    |    |_______________ Foreground color
    |____________________ Background color

Character cells are read from `0E000000` and drawn using the palette from `0E060000` and the font graphics at `0E060200` (depending on screen size and the bold bit). If the blink bit in the [ScreenMode register](registers.md#00001screenmode) is set, the most significant bit of a character cell will enable blinking for that cell instead of setting the background color to a value between 8 and 15.

## Bitmap modes

In both 16 and 256 color bitmap modes, the image is drawn from `0E000000` using the palette from `0E060000`. It is a very straightforward pair of modes, where the only gotcha is which order the pixels for 16-color mode are packed.

That order is right-left -- `1F` means a white pixel on the left, dark blue on the right.

---

TODO: finish this with the tilemap mode and everything about sprites, as pasted below.

### Tilemap cell
    PPPP vhTT TTTT TTTT
    |    |||_____________ Tile #
    |    ||______________ Horizontal flip
    |    |_______________ Vertical flip
    |____________________ Palette #

### Sprite
    PPPP EBBT TTTT TTTT
    |    || |____________ Tile #
    |    ||______________ Blend mode (0 off, 1 add, 2 subtract)
    |    |_______________ Enabled
    |____________________ Palette #
    PPP2 vhyx ...V VVVV VVVV ..HH HHHH HHHH
    |  | ||||    |             |___________  Horizontal position
    |  | ||||    |_________________________  Vertical position
    |  | ||||______________________________  Double width
    |  | |||_______________________________  Double height
    |  | ||________________________________  Horizontal flip
    |  | |_________________________________  Vertical flip
    |  |___________________________________  Double width and height
    |______________________________________  Priority

### Draw order

#### Tilemap mode
1. Backdrop
2. Sprites with priority 4
3. Map 0
4. Sprites with priority 3 
5. Map 1
6. Sprites with priority 2
7. Map 2
8. Sprites with priority 1
9. Map 3
10. Sprites with priority 0

#### Other modes:
1. Stuff
2. Sprites with any priority

/end TODO
