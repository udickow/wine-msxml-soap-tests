CXX=i686-pc-mingw32-g++
LDFLAGS=-static

PROGS=hello.exe

%.exe: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

all: $(PROGS)

clean:
	$(RM) $(PROGS)
