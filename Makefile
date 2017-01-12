DBG        = -ggdb3
OPTFLAGS   = -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes $(DBG) -pedantic
SDLVERSION = 1.2
ifeq ($(SDLVERSION),1.2)
SDLCONFIG = sdl-config
else ifeq ($(SDLVERSION),2)
SDLCONFIG = sdl2-config
$(info config to $(SDLCONFIG))
endif
SDL_LIBS   := $(shell $(SDLCONFIG) --static-libs)
SDL_CFLAGS := $(shell $(SDLCONFIG) --cflags)
CFLAGS     = $(OPTFLAGS)  $(SDL_CFLAGS) -std=c99
LDFLAGS    = $(SDL_LIBS) -lm #-lefence

$(info CFLAGS $(CFLAGS))
$(info LDFLAGS $(LDFLAGS))

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
