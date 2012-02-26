pdfpres: *.c *.h
	$(CC) -Wall -Wextra $(CFLAGS) $(LDLIBS) $(CPPFLAGS) \
	`pkg-config --cflags --libs gtk+-2.0 poppler-glib cairo` \
	`xml2-config --cflags --libs` \
	`./version.sh` \
	-o pdfpres *.c

.PHONY: clean
clean:
	rm -fv pdfpres
