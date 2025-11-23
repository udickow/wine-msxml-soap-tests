WINEPREFIX ?= $(HOME)/.wine
WINEROOT=/opt/wine
# WINEROOT may be overriden on the command line with e.g. 'make WINEROOT=/usr'
#   We add WINEROOT/include to include path so that wine/debug.h can be found
#   also on old self-built wines in /opt/ (modern wine 10.15 doesn't need it).
INCLUDES=-I$(WINEROOT)/include/wine/windows -I$(WINEROOT)/include

CCW=winegcc
CXXW=wineg++

CC=i686-pc-mingw32-gcc
CXX=i686-pc-mingw32-g++

# We don't need static if we just make sure wine sees the needed mingw32
# dll's, e.g. by symlinking them to windows/system32.
#LDFLAGS=-static
LDFLAGS=-lole32 -loleaut32 -luuid

CFLAGS=-O2
CCWEXTRA=-m32 -Wall -Wextra -Wno-sign-compare

PROGS=hello-c.exe.so tst-msxml_make_soap.exe.so tst-msxml_xmlns_simple.exe.so \
	tst-switch_strcmpW.exe.so

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
