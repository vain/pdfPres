EXECUTABLE=pdfpres
OBJECTS=prefs.o notes.o popplergdk.o
LIBS=gtk+-2.0 poppler-glib libxml-2.0

CFLAGS+=-Wall -Wextra `./version.sh` `pkg-config --cflags $(LIBS)`
LDFLAGS+=`pkg-config --libs $(LIBS)`

.PHONY: clean all

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(EXECUTABLE)
