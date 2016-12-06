# WS2812 Driver

This firmware implements a bit-banging driver for a string of WS2812 LEDs connected to a Creator Ci40 board.

## Hardware

The LED string should be connected to the Ci40 expansion port (https://docs.creatordev.io/ci40/guides/hardware/#primary-expansion-header) as follows:

| Ci40 Expansion Header (CN5) Pin | WS2812   |
| :-----------------------------: |:--------:|
| 2  (+5V)                        | +5V      |
| 39 (GND)                        | GND      |
| 8  (MFIO14)                     | DATA     |

### Jumpers
MFIO14 needs to be configured as IO rather than UART2 TX by moving jumper JP7 to bridge the 2 pins closest to the expansion header.

## Software

The remoteproc firmware is preconfigured to drive a string of 144 LEDs, but the length of the string can easily be changed with the NUM_LEDS define.
The timing of the WS2812's is done via the ws2812_delay function and a NS_TO_LOOPS macro to calculate the equivalent number of ticks of the MIPS coprocessor 0 timer for a given delay.
The pattern driven to the string is configured in the main loop.