DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
AR = $(TOOLCHAIN)/bin/i686-elf-ar
RANLIB = $(TOOLCHAIN)/bin/i686-elf-ranlib
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles

# The resulting static library will be moved into the sysroot by the 
# build-libs.py script.
libgui.a: libgui.c gui_scrollbar.c gui_text_input.c gui_text_view.c gui_view.c gui_slider.c utils.c gui_button.c gui_timer.c gui_scroll_view.c gui_layer.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libamc.a
	$(CC) $(CFLAGS) -o libgui.o -c libgui.c; \
	$(CC) $(CFLAGS) -o gui_scrollbar.o -c gui_scrollbar.c; \
	$(CC) $(CFLAGS) -o gui_text_input.o -c gui_text_input.c; \
	$(CC) $(CFLAGS) -o gui_text_view.o -c gui_text_view.c; \
	$(CC) $(CFLAGS) -o gui_view.o -c gui_view.c; \
	$(CC) $(CFLAGS) -o gui_slider.o -c gui_slider.c; \
	$(CC) $(CFLAGS) -o utils.o -c utils.c; \
	$(CC) $(CFLAGS) -o gui_button.o -c gui_button.c; \
	$(CC) $(CFLAGS) -o gui_timer.o -c gui_timer.c; \
	$(CC) $(CFLAGS) -o gui_scroll_view.o -c gui_scroll_view.c; \
	$(CC) $(CFLAGS) -o gui_layer.o -c gui_layer.c; \
	$(AR) rcs libgui.a libgui.o gui_scrollbar.o gui_text_input.o gui_text_view.o gui_view.o gui_slider.o utils.o gui_button.o gui_timer.o gui_scroll_view.o gui_layer.o; \
	$(RANLIB) libgui.a;
