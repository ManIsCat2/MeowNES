COMPILE_FOLDERS := src src/imgui src/imgui/backends
TARGET := MeowNES
BUILD_DIR := build
CXX := clang++
CXXFLAGS := -Wall -Wextra -O3 -Iinclude
LDFLAGS := -lSDL2 -lSDL2_image
ifeq ($(OS),Windows_NT)
	LDFLAGS += -lmingw32 -lSDL2main -lSDL2
endif

SOURCES := $(wildcard $(addsuffix /*.cpp,$(COMPILE_FOLDERS)))

OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	cp -r gui/ $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
