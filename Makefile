CC ?= cc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
BUILD_DIR ?= build
LIB_SOURCES := src/ring_buffer.c src/protocol.c src/filters.c
TEST_BIN := $(BUILD_DIR)/test_daq
HOST_BIN := $(BUILD_DIR)/uart_capture

.PHONY: all test host sanitize debug valgrind clean

all: test

$(BUILD_DIR):
	mkdir -p $@

$(TEST_BIN): $(LIB_SOURCES) tests/test_main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_SOURCES) tests/test_main.c -o $@

$(HOST_BIN): $(LIB_SOURCES) examples/uart_capture.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_SOURCES) examples/uart_capture.c -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

host: $(HOST_BIN)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer" test

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -O0 -g3 -Wall -Wextra -Werror -pedantic" $(TEST_BIN)

valgrind: $(TEST_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
