EXECUTABLE=pdfpres
OBJECTS=pdfpres.o prefs.o notes.o popplergdk.o
LIBS=gtk+-2.0 poppler-glib libxml-2.0

INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

prefix=/usr/local
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
datarootdir=$(prefix)/share
mandir=$(datarootdir)/man
man1dir=$(mandir)/man1

CFLAGS+=-Wall -Wextra `./version.sh` `pkg-config --cflags $(LIBS)`
LDFLAGS+=`pkg-config --libs $(LIBS)`

.PHONY: clean all install

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

install: all installdirs
	$(INSTALL_PROGRAM) $(EXECUTABLE) $(DESTDIR)$(bindir)/$(EXECUTABLE)
	$(INSTALL_DATA) man1/$(EXECUTABLE).1 $(DESTDIR)$(man1dir)/$(EXECUTABLE).1

installdirs:
	# Note: We only support GNU, OpenBSD and FreeBSD right now and all
	# of them know "mkdir -p".
	mkdir -p $(DESTDIR)$(bindir) $(DESTDIR)$(man1dir)

clean:
	rm -vf $(OBJECTS) $(EXECUTABLE)

dist:
	@echo Generating `git describe`.tar.gz
	git archive --prefix=`git describe`/ --format tar.gz HEAD > `git describe`.tar.gz
