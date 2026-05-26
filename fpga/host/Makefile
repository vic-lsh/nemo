#CFLAGS = -Wall -Werror -g -O3
CFLAGS = -Wall -Werror -g -O3 -mavx512f -march=native #-fno-builtin
LDFLAGS = -lnuma

SRC_DIR = src
BUILD_DIR = bin

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

BINARIES = $(patsubst $(SRC_DIR)/%.c, %, $(SRC_FILES))

all: $(BINARIES)

%: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LDFLAGS)

clean:
	rm -f $(BUILD_DIR)/*

.PHONY: all clean

