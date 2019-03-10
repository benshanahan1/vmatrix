# vmatrix

## build

Clone rpi-rgb-led-matrix library to Pi. Follow instructions to configure it properly for your hardware.

Update `RGB_LIB_DISTRIBUTION` in Makefile to point to where you cloned the rpi-rgb-led-matrix repo. This needs to be an **absolute** filepath.

Run `make` to build the vmatrix program.

The built program is placed in 'build/'. You can clean the build by running `make clean`.
