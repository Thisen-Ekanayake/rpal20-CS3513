## Makefile for the RPAL interpreter (CS 3513 Programming Project).
##
## Produces the executable `rpal20` in the project root.
## Build:    make
## Clean:    make clean
## Run:      ./rpal20 <source-file>

CXX       := g++
CXXFLAGS  := -std=c++17 -O2 -Wall -Wextra
LDFLAGS   :=

TARGET    := rpal20
SOURCES   := rpal20.cpp lexer.cpp parser.cpp ast.cpp standardizer.cpp cse.cpp
HEADERS   := token.h lexer.h parser.h ast.h standardizer.h cse.h
OBJECTS   := $(SOURCES:.cpp=.o)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
