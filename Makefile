# Poker Trainer - raylib / C
#
#   make            build ./poker  (finds raylib via pkg-config or Homebrew)
#   make run        build and run
#   make sim        build the headless bot-vs-bot simulator
#   make test       build and run the test suite
#   make nash       re-solve the push/fold ranges (rewrites src/pushfold.c, ~20 s)
#   make web        build the browser version into build/web (needs Emscripten;
#                   fetches and builds raylib for the web on first use)
#   make serve-web  play the web build at http://localhost:8000
#   make install-web WEB_DEST=dir   copy the web build into dir (e.g. a site's public/ folder)
#   make test-wasm  run the test suite compiled to WebAssembly (node, web stack size, overflow checks)
#   make test-web   browser tests of the web build in headless Chromium (installs Playwright on first use)
#   make RAYLIB_DIR=/path/to/raylib     use a raylib source checkout (static lib)

CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wno-missing-field-initializers
TARGET  := poker
ENGINE  := src/variants.c src/cards.c src/range.c src/equity.c src/game.c src/strategy.c src/ai.c src/basic.c src/pushfold.c
SRC     := src/main.c src/sound.c src/arrange.c src/glossary.c $(ENGINE)
UNAME_S := $(shell uname -s)

ifdef RAYLIB_DIR
  RL_CFLAGS := -I$(RAYLIB_DIR)/src
  RL_LIBS   := $(RAYLIB_DIR)/src/libraylib.a
else
  RL_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
  RL_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
  ifeq ($(strip $(RL_LIBS)),)
    ifeq ($(UNAME_S),Darwin)
      BREW      := $(shell brew --prefix 2>/dev/null)
      RL_CFLAGS := -I$(BREW)/include
      RL_LIBS   := -L$(BREW)/lib -lraylib
    else
      RL_LIBS   := -lraylib
    endif
  endif
endif

ifeq ($(UNAME_S),Darwin)
  SYS_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
              -framework CoreAudio -framework AudioToolbox
else
  SYS_LIBS := -lGL -lm -lpthread -ldl -lrt -lX11
endif

all: $(TARGET)

$(TARGET): $(SRC) src/*.h
	$(CC) $(CFLAGS) $(RL_CFLAGS) -o $@ $(SRC) $(RL_LIBS) $(SYS_LIBS) -lm

run: $(TARGET)
	./$(TARGET)

sim: tools/sim.c $(ENGINE) src/*.h
	$(CC) $(CFLAGS) -o sim tools/sim.c $(ENGINE) -lm

test: tests/test_main.c $(ENGINE) src/*.h
	$(CC) $(CFLAGS) -o test_poker tests/test_main.c src/arrange.c src/glossary.c $(ENGINE) -lm
	./test_poker

nash: tools/nash.c src/cards.c src/range.c src/*.h
	$(CC) $(CFLAGS) -o nash tools/nash.c src/cards.c src/range.c -lm
	./nash 6000 > src/pushfold.c

# ---- web (WebAssembly) build
EMCC          ?= emcc
RAYLIB_TAG    ?= 6.2
RAYLIB_WEB    ?= build/raylib
WEB_OUT       := build/web
WEB_FILES     := index.html index.js index.wasm index.data
WEB_ASSETS    := $(foreach f,ui_bold.otf ui_regular.otf,--preload-file assets/$(f)@assets/$(f))

$(RAYLIB_WEB)/src/libraylib.web.a:
	@test -d $(RAYLIB_WEB) || git clone --depth 1 --branch $(RAYLIB_TAG) https://github.com/raysan5/raylib.git $(RAYLIB_WEB)
	$(MAKE) -C $(RAYLIB_WEB)/src PLATFORM=PLATFORM_WEB

$(WEB_OUT)/index.html: $(SRC) src/*.h web/shell.html assets/*.otf $(RAYLIB_WEB)/src/libraylib.web.a
	@mkdir -p $(WEB_OUT)
	$(EMCC) -O2 -std=gnu11 -Wall -Wextra -Wno-missing-field-initializers -DPLATFORM_WEB \
	    -I$(RAYLIB_WEB)/src -o $@ $(SRC) $(RAYLIB_WEB)/src/libraylib.web.a \
	    -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB \
	    --shell-file web/shell.html $(WEB_ASSETS)

web: $(WEB_OUT)/index.html

serve-web: web
	cd $(WEB_OUT) && python3 -m http.server 8000

install-web: web
	@test -n "$(WEB_DEST)" || { echo "usage: make install-web WEB_DEST=path/to/dir"; exit 1; }
	mkdir -p $(WEB_DEST)
	cp $(addprefix $(WEB_OUT)/,$(WEB_FILES)) $(WEB_DEST)/

test-wasm: tests/test_main.c $(ENGINE) src/*.h
	@mkdir -p build
	$(EMCC) -O2 -std=gnu11 -Wall -Wextra -Wno-missing-field-initializers -o build/test_poker.js \
	    tests/test_main.c src/arrange.c src/glossary.c $(ENGINE) \
	    -sSTACK_SIZE=1MB -sSTACK_OVERFLOW_CHECK=2 -sASSERTIONS=1 -sALLOW_MEMORY_GROWTH=1
	node build/test_poker.js

tests/node_modules:
	cd tests && npm install --no-audit --no-fund && npx playwright install chromium

test-web: web tests/node_modules
	node tests/web_smoke.mjs

clean:
	rm -f $(TARGET) sim test_poker nash
	rm -rf $(WEB_OUT) build/test_poker.js build/test_poker.wasm

.PHONY: all run sim test test-wasm test-web nash web serve-web install-web clean
