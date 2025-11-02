CC = gcc
CFLAGS = -Wall -Wno-unused-function -D_GNU_SOURCE -I$(INCLUDE_DIR)
LDFLAGS = -Wall -ll -lfl

SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj
INCLUDE_DIR = src

EXEC = $(BIN_DIR)/tpcc

OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/tpc.tab.o $(OBJ_DIR)/tpc.yy.o \
       $(OBJ_DIR)/tree.o $(OBJ_DIR)/codegen.o $(OBJ_DIR)/semantic.o \
       $(OBJ_DIR)/symbol_table.o

all: $(EXEC)

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/tree.h $(SRC_DIR)/tpc.tab.h $(SRC_DIR)/tpc.yy.c $(SRC_DIR)/codegen.h $(SRC_DIR)/semantic.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJ_DIR)/tpc.tab.o: $(SRC_DIR)/tpc.tab.c $(SRC_DIR)/tpc.tab.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(SRC_DIR)/tpc.tab.c $(SRC_DIR)/tpc.tab.h: $(SRC_DIR)/tpc.y    	
	bison -d -Wcounterexamples -o $(SRC_DIR)/tpc.tab.c $(SRC_DIR)/tpc.y

$(OBJ_DIR)/tpc.yy.o: $(SRC_DIR)/tpc.yy.c $(SRC_DIR)/tpc.tab.h $(SRC_DIR)/tree.h | $(OBJ_DIR)
	$(CC)  -c -std=c2x $< -o $@ $(CFLAGS)

$(SRC_DIR)/tpc.yy.c: $(SRC_DIR)/tpc.lex $(SRC_DIR)/tpc.tab.h $(SRC_DIR)/tree.h
	flex -o $@ $<

$(OBJ_DIR)/codegen.o: $(SRC_DIR)/codegen.c $(SRC_DIR)/codegen.h $(SRC_DIR)/tree.h $(SRC_DIR)/symbol_table.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJ_DIR)/semantic.o: $(SRC_DIR)/semantic.c $(SRC_DIR)/semantic.h $(SRC_DIR)/tree.h $(SRC_DIR)/symbol_table.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJ_DIR)/symbol_table.o: $(SRC_DIR)/symbol_table.c $(SRC_DIR)/symbol_table.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/tree.h | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)/*.o $(SRC_DIR)/tpc.yy.c $(SRC_DIR)/tpc.tab.* $(EXEC)
