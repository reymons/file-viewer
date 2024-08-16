DEPS_DIR=$(PWD)/deps
LIBS_DIR=$(PWD)/libs
BUILD_DIR=$(PWD)/build
BUILD_DEPS_DIR=$(BUILD_DIR)/deps
SDL_DEPS=SDL2 SDL2_image
DEPS=$(SDL_DEPS)
CC=gcc
OUT=main
CC_INCLUDES=$(foreach DEP,$(SDL_DEPS),-I$(LIBS_DIR)/$(DEP)/include/SDL2)
CC_LIB_DIRS=$(foreach DEP,$(DEPS),-L$(LIBS_DIR)/$(DEP)/lib)
CC_LIBS=$(foreach DEP,$(DEPS),-l$(DEP))
CFLAGS=-O3 -Wall -Wextra -pedantic $(CC_INCLUDES) $(CC_LIB_DIRS) $(CC_LIBS)
SDL2_CONFIG=$(LIBS_DIR)/SDL2/bin/sdl2-config

all:
	$(CC) $(CFLAGS) -o $(OUT) main.c

init:
	git submodule update --remote
	export SDL2_CONFIG=$(SDL2_CONFIG); \
	make $(DEPS)

$(DEPS):
	@echo ""
	@echo "================================"
	@echo "Installing $@"
	@echo "================================"
	@echo ""
	mkdir -p $(BUILD_DEPS_DIR)/$@
	cd  $(BUILD_DEPS_DIR)/$@; \
	$(DEPS_DIR)/$@/configure --prefix $(LIBS_DIR)/$@; \
	make -j6; \
	make install

clean:
	rm -rf $(OUT)

clean_all:
	make clean
	rm -rf $(BUILD_DIR) $(LIBS_DIR)

