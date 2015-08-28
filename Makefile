CC=gcc
CXX=g++
DEFINES=
CFLAGS=-c -O2 -MMD
LDFLAGS=-s -lpcre
SOURCES=main.cxx
OBJECTS=$(SOURCES:.cxx=.o)
DEPENDENCIES=$(SOURCES:.cxx=.d)
EXECUTABLE=logan

.PHONY: all clean

all: $(EXECUTABLE)

-include $(DEPENDENCIES)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

clean :
	rm $(EXECUTABLE) $(OBJECTS) $(DEPENDENCIES)

%.o: %.cxx
	$(CXX) $(CFLAGS) $(DEFINES) $< -o $@
