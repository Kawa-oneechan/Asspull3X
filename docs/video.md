# Asspull IIIx Programmers Guide -- Video

The *Asspull IIIx* supports four different screen modes:

1. Text
2. 16-color bitmap
3. 256-color bitmap
4. Tilemaps

## Color palette

In all four modes, the palette can be freely manipulated. It is a single array of 256 16-bit values in the same basic format as on the Super NES, but with the byte order adjusted for use on a Motorola. It is stored at `0E060000` and can be accessed with the `PALETTE` define.

### Examples

Changing a single entry to white:

```c
PALETTE[15] = 0x7FFF;
```

Quickly uploading two separate 16-color strips:

```c
MISC->DmaCopy(PALETTE, (int16_t*)&tilesPal, 16, DMA_INT);
MISC->DmaCopy(PALETTE + 32, (int16_t*)&playerPal, 16, DMA_INT);
```

Note the `+ 32`, making `tilesPal` the 0th palette strip and `playerPal` the 1st.

## Text mode

In text mode, each cell is stored as a 16-bit value. Technically speaking it's the exact same format as in CGA-
compatible text mode, but with a few more lines in total.

    BBBB FFFF CCCC CCCC
    |    |    |__________ Character
    |    |_______________ Foreground color
    |____________________ Background color

Character cells are read from `0E000000` and the font graphics at `0E060200` (depending on screen size and the bold bit). If the blink bit in the [ScreenMode register](registers.md#00001reg_screenmode) is set, the most significant bit of a character cell will enable blinking for that cell instead of setting the background color to a value between 8 and 15.

## Bitmap modes

In both 16 and 256 color bitmap modes, the image is drawn from `0E000000`. It is a very straightforward pair of modes, where the only trick is which order the pixels for 16-color mode are packed, because again this is a Motorola CPU.

That order is right-left — `1F` means a white pixel on the *left*, dark blue on the *right*.

## Tile mode

In tile mode, you get four separate map layers that can be enabled via the [MapSet register](registers.md#00009reg_mapset). Each can be scrolled individually. A single map is 64×64 tiles in size. Tile graphics are taken from `0E050000`, in a four bit per pixel linear format, like on the Game Boy Advance. The maps are defined as `MAP1`  at `0E000000` through `MAP4` at `0E002000`, and the tileset as `TILESET`.

    PPPP vhTT TTTT TTTT
    |    |||_____________ Tile #
    |    ||______________ Horizontal flip
    |    |_______________ Vertical flip
    |____________________ Palette #

## Objects

Objects are available in all screen modes. Like tilemaps, their graphics are taken from `0E050000`. Their settings are stored in two separate arrays, one at `0E064000` and another at `0E064200`, defined as `OBJECTS_A` and `OBJECTS_B`.

    PPPP EBBT TTTT TTTT
    |    || |____________ Tile #
    |    ||______________ Blend mode (0 off, 1 add, 2 subtract)
    |    |_______________ Enabled
    |____________________ Palette #


    PPP2 vhyx ...V VVVV VVVV ..HH HHHH HHHH
    |  | ||||    |             |___________ Horizontal position
    |  | ||||    |_________________________ Vertical position
    |  | ||||______________________________ Double width
    |  | |||_______________________________ Double height
    |  | ||________________________________ Horizontal flip
    |  | |_________________________________ Vertical flip
    |  |___________________________________ Double up again
    |______________________________________ Priority

An object with the double width and height bits set would be functionally the same as an object with the double up bit set.

| Bits              | Size  |
| ----------------- | ----- |
| None              | 8x8   |
| Width             | 16x8  |
| Height            | 8x16  |
| Width and height  | 16x16 |
| Double up         | 16x16 |
| Double and width  | 32x16 |
| Double and height | 16x32 |
| All three         | 32x32 |

### Draw order

#### Tile mode

1. Backdrop
2. Objects with priority 4
3. Map 1
4. Objects with priority 3 
5. Map 2
6. Objects with priority 2
7. Map 3
8. Objects with priority 1
9. Map 4
10. Objects with priority 0

#### Other modes

1. Stuff
2. Objects with any priority

