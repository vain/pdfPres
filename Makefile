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

CFLAGS+=-Wall -Wextra `./version.sh` `pkg-config --cflags $(LIBS)`
LDFLAGS+=`pkg-config --libs $(LIBS)`

.PHONY: clean all install

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

install: all
	$(INSTALL_PROGRAM) -D $(EXECUTABLE) $(DESTDIR)$(bindir)/$(EXECUTABLE)
	$(INSTALL_DATA) -D man1/$(EXECUTABLE).1 $(DESTDIR)$(mandir)/man1/$(EXECUTABLE).1

clean:
	rm -vf $(OBJECTS) $(EXECUTABLE)

dist:
	@echo Generating `git describe`.tar.gz
	git archive --prefix=`git describe`/ --format tar.gz HEAD > `git describe`.tar.gz
