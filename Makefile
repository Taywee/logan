build = Release
objects = main.o
CC = g
CXX = g++

commonOptsAll = -Wall -Wextra
commonDebugOpts = -ggdb -O0 -DDEBUG
commonReleaseOpts = -s -O3
commonOpts = $(commonOptsAll) $(common$(build)Opts)

compileOptsAll = -c
compileOptsRelease =
compileOptsDebug =
compileOpts = $(commonOpts) $(compileOptsAll) $(compileOpts$(build))

linkerOptsAll = -static-libgcc -static-libstdc++
linkerOptsRelease =
linkerOptsDebug =
linkerOpts = $(commonOpts) $(linkerOptsAll) $(linkerOpts$(build))

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
