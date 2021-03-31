SRC_DIR=src
INCLUDE_DIR=include
OBJ_DIR=obj
BIN_DIR=bin

PARSE_DIR=parse
PARSE_SRC_DIR=$(PARSE_DIR)/src
PARSE_INCLUDE_DIR=$(PARSE_DIR)/include

C_CC=gcc
CPP_CC=g++
CXXFLAGS=-Wall -Werror -g
CPPFLAGS=-std=c++11
LDFLAGS=-lfl -lreadline -lutil

_OBJ=builtins.o cmdline.o exec.o path.o parse.o prompt.o shell.o scan.o
OBJ=$(patsubst %.o,$(OBJ_DIR)/%.o,$(_OBJ))

_PROG=shell
PROG=$(patsubst %,$(BIN_DIR)/%,$(_PROG))

all: $(PROG) Makefile

$(PROG): $(OBJ)
	$(CPP_CC) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CPP_CC) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ -I$(INCLUDE_DIR) -I$(PARSE_INCLUDE_DIR)

LEX=flex
LEX_IN=$(PARSE_DIR)/scan.l
LEX_SRC=$(patsubst $(PARSE_DIR)/%.l,$(PARSE_SRC_DIR)/%.yy.c,$(LEX_IN))
LEX_OBJ=$(patsubst $(PARSE_DIR)/%.l,$(OBJ_DIR)/%.o,$(LEX_IN))

YACC=bison
YACC_IN=$(PARSE_DIR)/parse.yy
YACC_SRC=$(patsubst $(PARSE_DIR)/%.yy,$(PARSE_SRC_DIR)/%.tab.cc,$(YACC_IN))
YACC_INCLUDE=$(patsubst $(PARSE_DIR)/%.yy,$(PARSE_INCLUDE_DIR)/%.tab.h,$(YACC_IN))
YACC_OBJ=$(patsubst $(PARSE_DIR)/%.yy,$(OBJ_DIR)/%.o,$(YACC_IN))

$(LEX_SRC): $(LEX_IN) $(YACC_INCLUDE)
	$(LEX) $(LEX_FLAGS) --outfile=$@ $<

$(YACC_SRC): $(YACC_IN)
	$(YACC) $(YACC_FLAGS) --output=$@ --defines=$(YACC_INCLUDE) $<

$(YACC_INCLUDE): $(YACC_SRC)

$(LEX_OBJ): $(LEX_SRC)
	$(C_CC) $(CXXFLAGS) -c $< -o $@ -I$(INCLUDE_DIR) -I$(PARSE_INCLUDE_DIR)

$(YACC_OBJ): $(YACC_SRC)
	$(CPP_CC) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ -I$(INCLUDE_DIR) -I$(PARSE_INCLUDE_DIR)

.PHONY: clean

clean:
	rm -f $(OBJ_DIR)/* $(BIN_DIR)/* $(LEX_SRC) $(YACC_SRC) $(YACC_INCLUDE)
