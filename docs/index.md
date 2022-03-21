# Asspull IIIx Programmers Guide

The structure of an *Asspull IIIx* ROM image and others are explained in the [Formats guide](formats.md).

Address space on the Motorola 68020 is 32-bits, but the *Asspull IIIx* only uses 28.

    $0FFFFFFF
      |_______ bank


This address space is divided into the following regions:

| address  | size     | name  |
| -------- | -------- | ----- |
| 00000000 | 00020000 | BIOS  |
| 00020000 | 00FE0000 | ROM   |
| 01000000 | 00400000 | RAM   |
| 013F0000 | 000FFFFF | STACK |
| 02000000 | 00080000 | DEV   |
| 0D000000 |          | REGS  |
| 0E000000 | 00080000 | VRAM  |

For the memory-mapped registers, please refer to the [Registers guide](registers.md).

Device memory is divided into sixteen blocks of `0x8000` bytes, or 32 kibibytes each. Each device but the last reports an identifying marker on the first two bytes of its block. How the rest of the block reacts depends on the device. The last device maps to on-cart battery-backed static RAM and has no identifying marker. Please refer to the [Devices guide](devices.md) for details.

Video memory is subdivided into the following regions:

| address  | content                              |
| -------- | ------------------------------------ |
| 0E000000 | Text, bitmap, and tile map data      |
| 0E050000 | Tile graphics                        |
| 0E060000 | Palette data                         |
| 0E060400 | Font graphics                        |
| 0E064000 | Object tile and palette data         |
| 0E064200 | Objection position and priority data |

Please refer to the [Video guide](video.md) for details.
