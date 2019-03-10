# vmatrix

**vmatrix** is a realtime audio visualizer for RGB matrix panels.

## Requirements

* Raspberry Pi 3
* RGB matrix panel(s)
* Adafruit RGB Matrix hat (optional, you can also manually wire the matrix to your Pi)
* 5V, 2A+ power supply
* USB sound adapter

## Build

Clone rpi-rgb-led-matrix library to Pi. Follow instructions to configure it properly for your hardware. If you're using the Adafruit RGB Matrix hat, you will need to modify the Makefile(s) to use that GPIO hardware description instead of the default one (see the [docs](https://github.com/hzeller/rpi-rgb-led-matrix#switch-the-pinout) for more information).

Update `RGB_LIB_DISTRIBUTION` in Makefile to point to where you cloned the rpi-rgb-led-matrix repo. This needs to be an **absolute** filepath.

Run `make` to build the vmatrix program.

The built program is placed in 'build/'. The built executable requires `sudo` to run.
