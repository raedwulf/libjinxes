# See LICENSE file for copyright and license details.

include config.mk

TERMS := $(shell toe -a | cut -f1 -s | egrep "^(xterm|rxvt|screen|linux|konsole|gnome|putty)[^+]")
#TERMS := $(shell toe -a | cut -f1 -s | grep "^[a-zA-Z0-9_-]\+ *$$")
SRC = jinxes.c
OBJ = $(SRC:.c=.o)

LIB = libjinxes.a
INC = jinxes.h

all: $(LIB) jinxestest

$(LIB): $(OBJ)
	$(AR) -rcs $@ $(OBJ)

jinxestest: jinxestest.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ jinxestest.o $(LIB)

.c.o:
	$(CC) $(CFLAGS) -c $<

${OBJ}: config.h config.mk terminfo.h

terminfo.h: terminfo.def.h terminfo.awk
	echo $(TERMS) | awk -f terminfo.awk > $@

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

install: $(LIB) $(INC) $(MAN)
	@echo @ install libjinxes to $(DESTDIR)$(PREFIX)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib
	@cp $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	@mkdir -p $(DESTDIR)$(PREFIX)/include
	@cp $(INC) $(DESTDIR)$(PREFIX)/include/$(INC)
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man3
	@cp jinxes.3 $(DESTDIR)$(PREFIX)/share/man/man3/jinxes.3

uninstall:
	@echo @ uninstall libjinxes from $(DESTDIR)$(PREFIX)
	@rm -f $(DESTDIR)$(PREFIX)/lib/$(LIB)
	@rm -f $(DESTDIR)$(PREFIX)/include/$(INC)
	@rm -f $(DESTDIR)$(PREFIX)/share/man/man3/rune.3

clean:
	rm -f $(LIB) jinxestest jinxestest.o terminfo.h $(OBJ)
