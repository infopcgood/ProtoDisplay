## Introduction

This repository contains the software for my protogen fursuit head, which has it's base on the amazing Delta Protogen head made by Coela Can't([@coelacant1](https://github.com/coelacant1)). This software is intended for use with the following electronics:

- 64x32 P3 HUB75 Matrix LED Panels x2
- Teensy 4.1 (initially without headers)
- SmartLED Shield for Teensy 4.1
- microSD card (FAT32)
- Raspberry Pi (Zero 2 W is preferred) with a camera attatched (runs a separate program, read below)
- HC-05 or HC-06 Bluetooth serial module
- 5V power supply or USB PD decoy trigger+XL4106 step down module (max to around 15A when very bright)

Feel free to use this software & contact me if you have any problems. Enjoy!

## Hardware Setup

Pictures coming soon. (TODO)
1. Solder the pin headers that came with the SmartLED Shield to the Teensy.
2. Insert the Teensy into the SmartLED Shield.
3. Insert the shield into a LED panel.
4. Daisy-chain two LED panels together.
5. Solder 4 wires or pin headers to the Pi's GPIO 2 or 4 (5V), 6/9/14/20/25/30/34/39 (GND), 8 (UART RX), 10 (UART TX).
6. Connect the Pi's RX and TX to Teensy's TX1 and RX1. **RX goes to TX and vice versa.**
7. Connect the Bluetooth serial module's 5V and GND with the power supply or the Teensy.
8. Connect the Bluetooth serial module's RXD and TXD to the Teensy's TX5 and RX5. **Again, RX to TX and TX to RX.**
9. Solder power lines.
10. Done! Move onto software setup.

## Software Setup

Unfinished (TODO)
1. Clone this repository and open it in VSCode or Codium.
2. Install the PlatformIO extension.
3. Build and upload this repository to the Teensy.

## Troubleshooting

TODO