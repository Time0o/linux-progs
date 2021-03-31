SRC_DIR=src
INC_DIR=include
OBJ_DIR=obj
BIN_DIR=bin

SRC=$(wildcard $(SRC_DIR)/*.c)
INC=$(wildcard $(INC_DIR)/*.h)
OBJ=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

CFLAGS=-std=c89 -pedantic -g -O3 \
        -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition

_progs=keygen otp_enc otp_dec otp_enc_d otp_dec_d
progs=$(patsubst %, $(BIN_DIR)/%, $(_progs))

all: $(progs)

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(OBJ_DIR)/util.o $(OBJ_DIR)/socket.o
	gcc $(CFLAGS) -o $@ $^

$(OBJ_DIR)/otp_enc.o: $(SRC_DIR)/otp_client.c $(INC)
	gcc $(CFLAGS) -DENC -c $< -o $@ -I$(INC_DIR)

$(OBJ_DIR)/otp_dec.o: $(SRC_DIR)/otp_client.c $(INC)
	gcc $(CFLAGS) -DDEC -c $< -o $@ -I$(INC_DIR)

$(OBJ_DIR)/otp_enc_d.o: $(SRC_DIR)/otp_server.c $(INC)
	gcc $(CFLAGS) -DENC -c $< -o $@ -I$(INC_DIR)

$(OBJ_DIR)/otp_dec_d.o: $(SRC_DIR)/otp_server.c $(INC)
	gcc $(CFLAGS) -DDEC -c $< -o $@ -I$(INC_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC)
	gcc $(CFLAGS) -DNDEBUG -c $< -o $@ -I$(INC_DIR)

.PHONY: clean

clean:
	rm -f $(OBJ_DIR)/* $(BIN_DIR)/*
