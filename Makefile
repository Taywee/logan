build = Release
objects = main.o
CC = g
CXX = g++

commonDebugOpts = -ggdb -O0 -DDEBUG
commonReleaseOpts = -s -O3
commonOpts = -Wall -Wextra
commonOpts += $(commonOptsAll) $(common$(build)Opts)

compileOptsRelease =
compileOptsDebug =
compileOpts = -c `pcre-config --cflags`
compileOpts += $(commonOpts) $(compileOptsAll) $(compileOpts$(build))

linkerOptsRelease =
linkerOptsDebug =
linkerOpts = /usr/lib/libpcre.a
linkerOpts += $(commonOpts) $(linkerOptsAll) $(linkerOpts$(build))

compile = $(CXX) $(compileOpts)
link = $(CXX) $(linkerOpts)


.PHONY : all clean

all : logan

clean :
	-rm -v logan $(objects)

logan : $(objects)
	$(link) -o logan $(objects)

main.o : main.cxx distance.hxx
	$(compile) -o main.o main.cxx 
