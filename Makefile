WINEPREFIX ?= $(HOME)/.wine
WINEROOT=/opt/wine
INCLUDES=-I$(WINEROOT)/include/wine/windows

CCW=winegcc
CXXW=wineg++

CC=i686-pc-mingw32-gcc
CXX=i686-pc-mingw32-g++

# We don't need static if we just make sure wine sees the needed mingw32
# dll's, e.g. by symlinking them to windows/system32.
#LDFLAGS=-static
LDFLAGS=-lole32 -loleaut32 -luuid

CCWEXTRA=-m32

#PROGS=hello-c.exe hello.exe tst-msxml_standalone-c.exe tst-msxml_standalone.exe
PROGS=hello-c.exe.so tst-msxml_standalone-c.exe.so

%.exe: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

%.exe: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

%.exe.so: %.c
	$(CCW) $(CFLAGS) $(CCWEXTRA) $(CPPFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

%.exe.so: %.cpp
	$(CXXW) $(CFLAGS) $(CCWEXTRA) $(CPPFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

all: $(PROGS)

clean:
	$(RM) $(PROGS)
