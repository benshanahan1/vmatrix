# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=/home/pi/code/rpi-rgb-led-matrix
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a

INCLUDES=-I$(RGB_INCDIR)
LIBRARIES=-L$(RGB_LIBDIR)
CFLAGS=-Wall -O3 -g -Wextra -Wno-unused-parameter
LDFLAGS+=$(LIBRARIES) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread -lstdc++ -lasound
SOURCES=kiss_fft.c kiss_fftr.c

BUILD_DIR=bin

all: $(RGB_LIBRARY) vmatrix generator

vmatrix:
	mkdir -p $(BUILD_DIR)
	gcc vmatrix.c -o $(BUILD_DIR)/vmatrix $(SOURCES) $(INCLUDES) $(LDFLAGS) $(CFLAGS)

generator:
	mkdir -p $(BUILD_DIR)
	gcc generator.c -o $(BUILD_DIR)/generator $(CFLAGS) -lm

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

clean:
	rm -rf $(BUILD_DIR)

FORCE:
.PHONY: FORCE
