all: xor-opt xor-dbg

CXXFLAGS = -Wall -g -O0
CXXFLAGS_OPT = -Os
SOURCES = chunk_detector.cc

xor-opt: $(SOURCES)
	g++ $(CXXFLAGS) $(CXXFLAGS_OPT) -o xor-opt $(SOURCES)

xor-dbg: $(SOURCES)
	g++ $(CXXFLAGS) -o xor-dbg $(SOURCES)

clean:
	rm -f xor-dbg xor-opt
