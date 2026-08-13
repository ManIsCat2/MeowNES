DIRS := src src/nes src/nes/mappers src/gb src/gb/mbcs src/filters
TARGET := EMeowlator
BUILD_DIR := build

ifeq ($(OS),Windows_NT)
    CXX := clang++
    CXXFLAGS := -Wall -Wextra -Wno-parentheses -O3 -Iinclude -static-libstdc++ -static-libgcc
    EXEEXT := .exe
	WINDEPLOYQT := $(shell command -v windeployqt6 2>/dev/null || command -v windeployqt 2>/dev/null)
else
    CXX := clang++
    CXXFLAGS := -Wall -Wextra -Wno-parentheses -O3 -Iinclude -g #-fsanitize=address -fsanitize=undefined
    EXEEXT :=
endif

LDFLAGS := $(shell pkg-config --libs Qt6Widgets sdl2) #-fsanitize=address -fsanitize=undefined
CXXFLAGS += $(shell pkg-config --cflags Qt6Widgets sdl2)

ifeq ($(OS),Windows_NT)
	LDFLAGS += resource.res
endif

BLUE := $(shell printf "\033[34m")
GREEN := $(shell printf "\033[32m")
WHITE := $(shell printf "\033[97m")
RESET := $(shell printf "\033[0m")

SOURCES := $(wildcard $(addsuffix /*.cpp,$(DIRS)))
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

ifeq ($(OS),Windows_NT)
all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)$(EXEEXT) deploydll
else
all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)
endif

$(BUILD_DIR)/$(TARGET)$(EXEEXT): $(OBJECTS)
	@echo "$(BLUE)Linking object files...$(RESET)"
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)Build complete!$(RESET)"

$(BUILD_DIR)/%.o: %.cpp
	@echo "$(BLUE)Compiling: $(GREEN)$<$(WHITE) -> $(GREEN)$@$(RESET)"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	@echo "$(BLUE)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR)

deploydll: $(BUILD_DIR)/$(TARGET)$(EXEEXT) # only for windows!
	@if [ "$(OS)" = "Windows_NT" ]; then \
		echo "$(BLUE)Running windeployqt to $(BUILD_DIR)/...$(RESET)"; \
		$(WINDEPLOYQT) --release --compiler-runtime --dir $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)$(EXEEXT); \
	fi
	@echo "$(BLUE)Copying MinGW runtime dependencies...$(RESET)"
	@ntldd -R "$(BUILD_DIR)/$(TARGET)$(EXEEXT)" | \
	grep -i "mingw64" | \
	sed -E 's/.*=> //g' | \
	sed -E 's/ \(0x.*\)//g' | \
	tr '\\\\' '/' | \
	sed -E 's|^[A-Za-z]:||' | \
	sed -E 's|^/.*?/mingw64|/mingw64|' | \
	tr -d '\r' | \
	while read dll; do \
		cp -u "$$dll" "$(BUILD_DIR)/"; \
	done

.PHONY: all clean