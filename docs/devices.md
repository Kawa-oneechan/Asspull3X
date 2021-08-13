# Asspull IIIx Programmers Guide -- Devices

Each of the sixteen blocks of `8000` bytes starting from `2000000` may or may not map to a device. The first two bytes of each block identify what kind of device it is. If those bytes are the value `0000` or `FFFF` there is no device.

### Disk drive

The disk drive is identified by the value `0144`.  The next `uint16` value selects which sector to read or write (IO register `00030` before) and the next byte controls the device (`00032` before):

    ...B WREP
       | ||||__ Disk present (read only)
       | |||___ Error state (read only)
       | ||____ Read now
       | |_____ Write now
       |_______ Busy state (read only)

From the 512th byte on, another 512 bytes form the disk controller's internal RAM, used to hold a sector's worth of data to read or write. That leaves plenty room for expansion.

### Line printer

Identified by the value `4C50`, writing to the next byte pipes directly to the printer. Reading it returns `00` or an error value, to be determined. *This might make a nice alternative to `REG_DEBUGOUT`...*