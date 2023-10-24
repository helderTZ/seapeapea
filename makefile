CXX=clang
CFLAGS=-I/usr/lib/llvm-10/include/
LDFLAGS=-lclang

all:	seapeapea

seapeapea:	main.cpp
	$(CXX) -o $@ main.cpp $(CFLAGS) $(LDFLAGS)

clean:
	if [ -f "seapeapea" ]; then rm -f seapeapea; fi

.PHONY: clean all
