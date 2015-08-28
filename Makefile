CC=gcc
CXX=g++
DESTDIR=/usr
DEFINES=
CFLAGS=-c -O2 -MMD
LDFLAGS=-s -lpcre
SOURCES=main.cxx
OBJECTS=$(SOURCES:.cxx=.o)
DEPENDENCIES=$(SOURCES:.cxx=.d)
EXECUTABLE=logan

.PHONY: all clean install uninstall

all: $(EXECUTABLE)

-include $(DEPENDENCIES)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

install: $(EXECUTABLE)
	install -d $(DESTDIR)/bin
	install -d $(DESTDIR)/share/man/man1
	install $(EXECUTABLE) $(DESTDIR)/bin/
	gzip -c doc/$(EXECUTABLE).1 > $(DESTDIR)/share/man/man1/$(EXECUTABLE).1.gz

uninstall:
	-rm $(DESTDIR)/bin/$(EXECUTABLE) $(DESTDIR)/share/man/man1/$(EXECUTABLE).1.gz

clean :
	rm $(EXECUTABLE) $(OBJECTS) $(DEPENDENCIES)

%.o: %.cxx
	$(CXX) $(CFLAGS) $(DEFINES) $< -o $@
