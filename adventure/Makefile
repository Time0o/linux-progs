SRC_DIR=src
INC_DIR=include
OBJ_DIR=obj
BIN_DIR=bin

SRC=$(wildcard $(SRC_DIR)/*.c)
INC=$(wildcard $(INC_DIR)/*.h)
OBJ=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

CFLAGS=-std=c89 -pedantic -g -O3 \
        -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition

$(BIN_DIR)/adventure: $(OBJ)
	gcc $(CFLAGS) -o $@ $^ -lreadline

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC)
	gcc $(CFLAGS) -c $< -o $@ -I$(INC_DIR)

.PHONY: clean

clean:
	rm -f $(OBJ_DIR)/* $(BIN_DIR)/*
