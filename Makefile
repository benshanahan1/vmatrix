# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=/home/pi/code/rpi-rgb-led-matrix
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread -lstdc++

BUILD_DIR=build

CFLAGS=-Wall -O3 -g -Wextra -Wno-unused-parameter

all: $(RGB_LIBRARY)
	mkdir -p $(BUILD_DIR)
	gcc main.c -o $(BUILD_DIR)/main -I$(RGB_INCDIR) $(LDFLAGS) $(CFLAGS)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

clean:
	rm -rf $(BUILD_DIR)

FORCE:
.PHONY: FORCE
