CC = gcc
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=
CFLAGS := $(shell pkg-config --cflags glib-2.0 gio-2.0 gtk+-3.0 gtkhex-3) -Wall -g -ansi -std=c99 $(EXTRA_CFLAGS)
LDFLAGS = $(EXTRA_LDFLAGS) -Wl,--as-needed
LDADD := $(shell pkg-config --libs glib-2.0 gio-2.0 gtk+-3.0 gthread-2.0 gtkhex-3)
OBJECTS = guart.o conf.o serial.o
DEPFILES = $(foreach m,$(OBJECTS:.o=),.$(m).m)

.PHONY : clean distclean all
%.o : %.c
	$(CC) $(CFLAGS) -c $<

.%.m : %.c
	$(CC) $(CFLAGS) -M -MF $@ -MG $<

all: guart

guart: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LDADD)

clean:
	rm -f *.o *.*.m

distclean : clean
	rm -f .*.m
	rm -f guart

install: guart
	install guart $(DESTDIR)/usr/bin

NODEP_TARGETS := clean distclean
depinc := 1
ifneq (,$(filter $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 0
endif
ifneq (,$(fitler-out $(NODEP_TARGETS),$(MAKECMDGOALS)))
depinc := 1
endif

ifeq ($(depinc),1)
-include $(DEPFILES)
endif
