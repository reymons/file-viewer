DEPS_DIR=$(PWD)/deps
BUILD_DIR=$(PWD)/build
BUILD_DIR_DEPS=$(BUILD_DIR)/deps
LIBSDL_DIR=$(PWD)/lib/SDL
CC=gcc
OUT=main
CFLAGS=-Os -Wall -Wextra -pedantic -I$(LIBSDL_DIR)/include -L$(LIBSDL_DIR)/lib -lSDL2

all:
	$(CC) $(CFLAGS) -o $(OUT) main.c

init:
	git submodule update --remote
	mkdir -p $(BUILD_DIR_DEPS)/SDL
	cd $(BUILD_DIR_DEPS)/SDL && \
		$(DEPS_DIR)/SDL/configure --prefix $(LIBSDL_DIR) && \
		make -j6 && \
		make install

clean:
	rm -rf $(OUT)

