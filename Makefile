CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS  :=

TARGET   := bin/competicao
BUILD_DIR := build
SRC_DIR  := src
INC_DIRS := -Iinclude -Isrc

SRCS := $(shell find $(SRC_DIR) -type f -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all debug release clean run

all: release

release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGET)

debug: CXXFLAGS := -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -MMD -MP
debug: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_DIRS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) bin

run: $(TARGET)
	./$(TARGET)