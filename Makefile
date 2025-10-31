DIR = $(shell find inc -type d)
BINDIR = $(DIR:inc%=bin%)
LIBS = glfw3 gl liburing
LIBCPPFLAGS = $(shell pkg-config --cflags $(LIBS))
LIBLDFLAGS = $(shell pkg-config --libs $(LIBS))
SAN =
CPPFLAGS = -MMD -MP -Ibin -Iinc $(LIBCPPFLAGS)
DEBUG = -ggdb3
OPT ?=
CFLAGS = $(SAN) $(DEBUG) $(OPT)
CXXFLAGS = -std=c++20 $(SAN) $(DEBUG) $(OPT)
LDFLAGS = $(SAN) $(LIBLDFLAGS) -lm

CC = gcc
CXX = g++
LD = g++
RM = rm

BIN = main
BIN_PATH = $(BIN:%=bin/%)

HDR = $(shell find inc -type f)
GCH = $(HDR:inc/%=bin/%.gch)

SRC = $(shell find src -type f -regex ".*\.\(c\|cpp\)")
SRC_NOMAIN = $(filter-out $(BIN:%=src/%.cpp),$(SRC))
OBJ = $(SRC:src/%=bin/%.o)
OBJ_NOMAIN = $(SRC_NOMAIN:src/%=bin/%.o)

DEP = $(SRC:src/%=bin/%.d) $(HDR:inc/%=bin/%.d)

all:: $(BINDIR) $(GCH) $(BIN_PATH)

$(BIN_PATH): bin/%: bin/%.cpp.o $(OBJ_NOMAIN)
	$(LD) -o $@ $^ $(LDFLAGS)

bin/%.c.o: src/%.c
	$(CC) $(CPPFLAGS) -c -o $@ $< $(CFLAGS)

bin/%.cpp.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $< $(CXXFLAGS)

bin/%.h.gch: inc/%.h
	$(CXX) $(CPPFLAGS) -c -o $@ $< $(CFLAGS)

bin/%.hpp.gch: inc/%.hpp
	$(CXX) $(CPPFLAGS) -c -o $@ $< $(CXXFLAGS)

run:: run-main

run-%:: all
	$(@:run-%=bin/%)

clean::
	$(RM) -rf bin

$(BINDIR): %:
	mkdir -p $@

-include $(DEP)

