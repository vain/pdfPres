EXECUTABLE=pdfpres
OBJECTS=pdfpres.o prefs.o notes.o popplergdk.o
LIBS=gtk+-2.0 poppler-glib libxml-2.0

INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

CFLAGS+=-Wall -Wextra `./version.sh` `pkg-config --cflags $(LIBS)`
LDFLAGS+=`pkg-config --libs $(LIBS)`

.PHONY: clean all install

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

install: all
	$(INSTALL_PROGRAM) -D $(EXECUTABLE) $(DESTDIR)/usr/bin/$(EXECUTABLE)
	$(INSTALL_DATA) -D man1/$(EXECUTABLE).1 $(DESTDIR)/usr/share/man/man1/$(EXECUTABLE).1

clean:
	rm -vf $(OBJECTS) $(EXECUTABLE)
