# install dependencies (Ubuntu/Debian): `apt install libclang-dev`

CXX=clang++
CFLAGS=-I/usr/lib/llvm-10/include/ -std=c++17
LDFLAGS=-lclang

all:	seapeapea

seapeapea:	main.cpp makefile
	$(CXX) -o $@ main.cpp $(CFLAGS) $(LDFLAGS)

clean:
	if [ -f "seapeapea" ]; then rm -f seapeapea; fi

.PHONY: clean all
