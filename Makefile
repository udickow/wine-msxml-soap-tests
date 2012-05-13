WINEPREFIX ?= $(HOME)/.wine
WINEROOT=/opt/wine
INCLUDES=-I$(WINEROOT)/include/wine/windows

CXX=i686-pc-mingw32-g++
# We don't need static if we just make sure wine sees the needed mingw32
# dll's, e.g. by symlinking them to windows/system32.
#LDFLAGS=-static

PROGS=hello.exe tst-msxml_standalone.exe

%.exe: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

all: $(PROGS)

clean:
	$(RM) $(PROGS)
