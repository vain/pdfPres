pdfpres: *.c *.h
	$(CC) -Wall -Wextra $(CFLAGS) $(CPPFLAGS) \
	-o pdfpres *.c \
	`./version.sh` \
	`pkg-config --cflags --libs gtk+-3.0 poppler-glib cairo` \
	`xml2-config --cflags --libs` \
	 $(LDLIBS)

.PHONY: clean
clean:
	rm -fv pdfpres
