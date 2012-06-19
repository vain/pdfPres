EXECUTABLE=pdfpres
OBJECTS=pdfpres.o prefs.o notes.o popplergdk.o
LIBS=gtk+-2.0 poppler-glib libxml-2.0

CFLAGS+=-Wall -Wextra `./version.sh` `pkg-config --cflags $(LIBS)`
LDFLAGS+=`pkg-config --libs $(LIBS)`

.PHONY: clean all

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

clean:
	rm -vf $(OBJECTS) $(EXECUTABLE)
