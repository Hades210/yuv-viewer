DBG        = -ggdb3
OPTFLAGS   = -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes $(DBG) -pedantic
SDL_LIBS   := $(shell sdl-config --static-libs)
SDL_CFLAGS := $(shell sdl-config --cflags)
CFLAGS     = $(OPTFLAGS)  $(SDL_CFLAGS) -std=c99
LDFLAGS    = $(SDL_LIBS) -lm #-lefence

SRC        = yv.c
TARGET     = yv
OBJ        = $(SRC:.c=.o)
INSTALLDIR = $(HOME)/.local/bin

default: $(TARGET)

%.o: %.c Makefile
	$(CC) $(CFLAGS)  -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -rf $(OBJ) $(TARGET) core*

install: yv
	[ -d $(INSTALLDIR) ] || mkdir $(INSTALLDIR)
	mv yv $(INSTALLDIR)

test: yv
	./yv cif.yuv 352 288 nv12

stat:
	cloc .
check:
	cppcheck .
style:
	uncrustify *.c --replace --no-backup
