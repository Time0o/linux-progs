SRC_DIR=src
OBJ_DIR=obj
BIN_DIR=bin

_OBJ=main.o
OBJ=$(patsubst %.o, $(OBJ_DIR)/%.o, $(_OBJ))

CFLAGS=-Wall -Werror -std=gnu99

$(BIN_DIR)/find: $(OBJ)
	gcc $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	gcc $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	@rm -f $(OBJ_DIR)/* $(BIN_DIR)/*
